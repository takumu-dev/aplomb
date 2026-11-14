#include "session.hpp"

#include "contact_tracker.hpp"
#include "playback.hpp"
#include <aplomb/core/assertion.hpp>
#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/version.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/scenes/build_scene.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Far enough apart that characters of a human's size fall without touching each other.
constexpr float place_spacing = 1.0F;

// How high above the ground the rig's space is placed, so that a rig built standing on its origin
// falls a little before it lands.
constexpr float drop_height = 0.5F;

// The name of the scene a session is built on unless a replay names another.
constexpr std::string_view default_scene = "flat_ground";

// Where a place is along the row: the first at the origin, then alternately to either side of it,
// so that the characters stay gathered where the camera starts.
JPH::Vec3 place_position(std::size_t place) {
    const std::size_t places_out = (place + 1) / 2;
    const float distance = static_cast<float>(places_out) * place_spacing;
    return {place % 2 == 1 ? distance : -distance, drop_height, 0.0F};
}

// Why a replay can't be played back with a rig, or nothing if it can: the world would refuse a
// push to a character or a part that the replay or its rig doesn't have.
std::optional<std::string> unknown_push_target(const aplomb::ReplayDesc& replay,
                                               const aplomb::RigDesc& rig) {
    const std::size_t characters = replay.initial.characters.size();
    for (std::size_t step = 1; step <= replay.steps.size(); ++step) {
        for (const aplomb::Push& push : replay.steps[step - 1].pushes) {
            if (push.character.index() >= characters) {
                return std::format("step {} of the replay pushes character {}, but the replay has "
                                   "{} character{}",
                                   step, push.character.index(), characters,
                                   characters == 1 ? "" : "s");
            }
            if (push.part.index() >= rig.parts.size()) {
                return std::format(R"(step {} of the replay pushes part {}, but the rig "{}" has )"
                                   "{} parts",
                                   step, push.part.index(), rig.name, rig.parts.size());
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::expected<Session, aplomb::WorldError> Session::create(const aplomb::WorldSettings& settings) {
    std::expected<aplomb::World, aplomb::WorldError> world = aplomb::World::create(settings);
    if (!world) {
        return std::unexpected(std::move(world.error()));
    }
    aplomb::build_flat_ground(world->physics_system());
    return Session(std::move(*world), settings);
}

std::expected<Session, std::string> Session::record(const aplomb::WorldSettings& settings,
                                                    const aplomb::RigDesc& rig,
                                                    std::uint64_t rig_file_hash,
                                                    std::size_t characters) {
    std::expected<Session, aplomb::WorldError> session = create(settings);
    if (!session) {
        return std::unexpected(std::format("{}", session.error()));
    }
    ReplayStart start{.rig_file_hash = rig_file_hash, .initial = {}};
    for (std::size_t place = 0; place < characters; ++place) {
        const std::expected<aplomb::CharacterId, aplomb::CharacterError> added =
            session->add_character(rig);
        if (!added) {
            return std::unexpected(std::format("{}", added.error()));
        }
        start.initial.characters.push_back({.root_position = place_position(place)});
    }
    start.initial.state_hash = session->world_.state_hash();
    session->replay_start_ = std::move(start);
    session->world_.start_recording();
    return std::move(*session);
}

std::expected<Session, std::string> Session::play(const aplomb::WorldSettings& settings,
                                                  aplomb::ReplayDesc replay,
                                                  const aplomb::RigDesc& rig,
                                                  std::uint64_t rig_file_hash) {
    // A replay played back with another rig than its own would differ for no reason worth finding.
    if (rig_file_hash != replay.rig_file_hash) {
        return std::unexpected(
            std::format("the replay was recorded with a rig file whose hash is {:016x}, but the "
                        "loaded rig file's is {:016x}",
                        replay.rig_file_hash, rig_file_hash));
    }
    if (std::optional<std::string> problem = unknown_push_target(replay, rig)) {
        return std::unexpected(std::move(*problem));
    }
    aplomb::WorldSettings replay_settings = settings;
    replay_settings.step_rate = replay.step_rate;
    std::expected<aplomb::World, aplomb::WorldError> world = aplomb::World::create(replay_settings);
    if (!world) {
        return std::unexpected(std::format("{}", world.error()));
    }
    if (!aplomb::build_scene(replay.scene, world->physics_system())) {
        return std::unexpected(std::format(R"(there is no scene "{}")", replay.scene));
    }
    Session session(std::move(*world), replay_settings);
    for (const aplomb::InitialState& initial : replay.initial.characters) {
        const std::expected<aplomb::CharacterId, aplomb::CharacterError> added =
            session.world_.add_character(rig, {.position = initial.root_position});
        if (!added) {
            return std::unexpected(std::format("{}", added.error()));
        }
        session.characters_.push_back(*added);
        // A character standing at a place in the row holds it, so that none added once the replay
        // ends lands on it.
        for (std::size_t place = 0; place < replay.initial.characters.size(); ++place) {
            if (place_position(place) == initial.root_position) {
                session.places_.resize(std::max(session.places_.size(), place + 1));
                session.places_[place] = *added;
                break;
            }
        }
    }
    Playback playback{.steps = replay.steps.size(), .played = 0, .first_differing_step = {}};
    if (const std::uint64_t hash = session.world_.state_hash(); hash != replay.initial.state_hash) {
        playback.first_differing_step = {
            .step = 0,
            .state_hashes = {replay.initial.state_hash, hash},
        };
    }
    session.replay_ = std::move(replay);
    session.playback_ = playback;
    return session;
}

Session::Session(aplomb::World world, const aplomb::WorldSettings& settings)
    : contacts_(std::make_unique<ContactTracker>()), world_(std::move(world)), settings_(settings) {
    world_.physics_system().SetContactListener(contacts_.get());
}

Session& Session::operator=(Session&& other) noexcept {
    // The world goes before the listener its physics system points at.
    world_ = std::move(other.world_);
    contacts_ = std::move(other.contacts_);
    settings_ = other.settings_;
    characters_ = std::move(other.characters_);
    places_ = std::move(other.places_);
    steps_taken_ = other.steps_taken_;
    replay_start_ = std::move(other.replay_start_);
    replay_ = std::move(other.replay_);
    playback_ = other.playback_;
    return *this;
}

std::expected<aplomb::CharacterId, aplomb::CharacterError>
Session::add_character(const aplomb::RigDesc& rig) {
    APLOMB_ASSERT(can_change_characters(),
                  "a session adds no character while it records or plays a replay back");
    const auto free = std::ranges::find_if(
        places_, [](const std::optional<aplomb::CharacterId>& held) { return !held.has_value(); });
    const auto place = static_cast<std::size_t>(std::distance(places_.begin(), free));
    std::expected<aplomb::CharacterId, aplomb::CharacterError> character =
        world_.add_character(rig, {.position = place_position(place)});
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
    APLOMB_ASSERT(can_change_characters(),
                  "a session removes no character while it records or plays a replay back");
    const aplomb::Character& removed = world_.character(character);
    for (std::size_t part = 0; part < removed.rig().parts.size(); ++part) {
        contacts_->forget_body(removed.part_body(aplomb::PartId(static_cast<std::uint32_t>(part))));
    }
    world_.remove_character(character);
    std::erase(characters_, character);
    const auto held = std::ranges::find(places_, std::optional(character));
    if (held != places_.end()) {
        held->reset();
    }
}

void Session::push(aplomb::CharacterId character, aplomb::PartId part, JPH::Vec3 world_point,
                   JPH::Vec3 impulse) {
    APLOMB_ASSERT(!playing(), "a session playing a replay back takes no pushes but the replay's");
    world_.push(character, part, world_point, impulse);
}

std::span<const aplomb::CharacterId> Session::characters() const noexcept { return characters_; }

bool Session::can_change_characters() const noexcept { return !recording() && !playing(); }

void Session::step() {
    if (!playback_ || playback_->finished()) {
        world_.step();
        ++steps_taken_;
        return;
    }
    const aplomb::RecordedStep& next = replay_.steps[playback_->played];
    for (const aplomb::Push& push : next.pushes) {
        world_.push(characters_[push.character.index()], push.part, push.world_point, push.impulse);
    }
    world_.step();
    ++steps_taken_;
    ++playback_->played;
    if (const std::uint64_t hash = world_.state_hash();
        hash != next.state_hash && !playback_->first_differing_step) {
        playback_->first_differing_step = {
            .step = playback_->played,
            .state_hashes = {next.state_hash, hash},
        };
    }
}

std::uint64_t Session::steps_taken() const noexcept { return steps_taken_; }

float Session::step_rate() const noexcept { return settings_.step_rate; }

const aplomb::WorldSettings& Session::settings() const noexcept { return settings_; }

bool Session::recording() const noexcept { return replay_start_.has_value(); }

aplomb::ReplayDesc Session::stop_recording() {
    std::optional<ReplayStart> start = std::exchange(replay_start_, std::nullopt);
    APLOMB_ASSERT(start.has_value(), "the session records itself as a replay");
    std::vector<aplomb::RecordedStep> steps = world_.stop_recording();
    // A replay names each character by its place among the replay's characters, not by its id.
    for (aplomb::RecordedStep& step : steps) {
        for (aplomb::Push& push : step.pushes) {
            const auto index =
                std::distance(characters_.begin(), std::ranges::find(characters_, push.character));
            push.character = aplomb::CharacterId(static_cast<std::uint32_t>(index));
        }
    }
    return {
        .aplomb_version = std::string(aplomb::version()),
        .jolt_version = std::string(aplomb::jolt_version()),
        .rig_file_hash = start->rig_file_hash,
        .step_rate = settings_.step_rate,
        // The sandbox's world derives nothing from a seed.
        .seed = 0,
        .scene = std::string(default_scene),
        .initial = std::move(start->initial),
        .steps = std::move(steps),
    };
}

const std::optional<Playback>& Session::playback() const noexcept { return playback_; }

bool Session::playing() const noexcept { return playback_ && !playback_->finished(); }

const aplomb::World& Session::world() const noexcept { return world_; }

std::vector<Contact> Session::contacts() const { return contacts_->contacts(); }
