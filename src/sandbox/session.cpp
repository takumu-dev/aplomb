#include "session.hpp"

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
    return Session(std::move(*world), settings.step_rate);
}

Session::Session(aplomb::World world, float step_rate) noexcept
    : world_(std::move(world)), step_rate_(step_rate) {}

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
    world_.remove_character(character);
    std::erase(characters_, character);
    std::ranges::find_if(places_, [&](const std::optional<aplomb::CharacterId>& held) {
        return held == character;
    })->reset();
}

std::span<const aplomb::CharacterId> Session::characters() const noexcept { return characters_; }

void Session::step() {
    world_.step();
    ++steps_taken_;
}

std::uint64_t Session::steps_taken() const noexcept { return steps_taken_; }

float Session::step_rate() const noexcept { return step_rate_; }

const aplomb::World& Session::world() const noexcept { return world_; }
