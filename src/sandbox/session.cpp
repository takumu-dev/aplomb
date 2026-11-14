#include "session.hpp"

#include "contact_tracker.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {

// Far enough apart that characters of a human's size fall without touching each other.
constexpr float place_spacing = 1.0F;

// How high above the ground the rig's space is placed, so that a rig built standing on its origin
// falls a little before it lands.
constexpr float drop_height = 0.5F;

// Where a place is along the row: the first at the origin, then alternately to either side of it,
// so that the characters stay gathered where the camera starts.
float place_x(std::size_t place) {
    const std::size_t places_out = (place + 1) / 2;
    const float distance = static_cast<float>(places_out) * place_spacing;
    return place % 2 == 1 ? distance : -distance;
}

} // namespace

std::expected<Session, aplomb::WorldError> Session::create(const aplomb::WorldSettings& settings) {
    std::expected<aplomb::World, aplomb::WorldError> world = aplomb::World::create(settings);
    if (!world) {
        return std::unexpected(std::move(world.error()));
    }
    aplomb::build_flat_ground(world->physics_system());
    auto contacts = std::make_unique<ContactTracker>();
    world->physics_system().SetContactListener(contacts.get());
    return Session(std::move(*world), std::move(contacts), settings.step_rate);
}

Session::Session(aplomb::World world, std::unique_ptr<ContactTracker> contacts,
                 float step_rate) noexcept
    : contacts_(std::move(contacts)), world_(std::move(world)), step_rate_(step_rate) {}

std::expected<aplomb::CharacterId, aplomb::CharacterError>
Session::add_character(const aplomb::RigDesc& rig) {
    const auto free = std::ranges::find_if(
        places_, [](const std::optional<aplomb::CharacterId>& held) { return !held.has_value(); });
    const auto place = static_cast<std::size_t>(std::distance(places_.begin(), free));
    const aplomb::Transform root{.position = JPH::Vec3(place_x(place), drop_height, 0.0F)};
    std::expected<aplomb::CharacterId, aplomb::CharacterError> character =
        world_.add_character(rig, root);
    if (character) {
        characters_.push_back(*character);
        if (free == places_.end()) {
            places_.emplace_back(*character);
        } else {
            *free = *character;
        }
    }
    return character;
}

void Session::remove_character(aplomb::CharacterId character) {
    const aplomb::Character& removed = world_.character(character);
    for (std::size_t part = 0; part < removed.rig().parts.size(); ++part) {
        contacts_->forget_body(removed.part_body(aplomb::PartId(static_cast<std::uint32_t>(part))));
    }
    world_.remove_character(character);
    std::erase(characters_, character);
    std::ranges::find_if(places_, [&](const std::optional<aplomb::CharacterId>& held) {
        return held == character;
    })->reset();
}

void Session::push(aplomb::CharacterId character, aplomb::PartId part, JPH::Vec3 world_point,
                   JPH::Vec3 impulse) {
    world_.push(character, part, world_point, impulse);
}

std::span<const aplomb::CharacterId> Session::characters() const noexcept { return characters_; }

void Session::step() {
    world_.step();
    ++steps_taken_;
}

std::uint64_t Session::steps_taken() const noexcept { return steps_taken_; }

float Session::step_rate() const noexcept { return step_rate_; }

const aplomb::World& Session::world() const noexcept { return world_; }

std::vector<Contact> Session::contacts() const { return contacts_->contacts(); }
