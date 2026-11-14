#pragma once

#include "contact_tracker.hpp"
#include "playback.hpp"
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/files/replay_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// What the sandbox shows: a world that owns its physics system, built on a scene, flat_ground
/// unless a replay names another, and the characters the owner adds onto it, with a contact
/// listener on its physics system that keeps the contacts there.
///
/// Each character is dropped from just above the ground at a place of its own in a row along the X
/// axis, the place nearest the origin that no other character holds.
///
/// A session can record itself as a replay from its start, or play a replay back, sending each of
/// its steps' pushes as it steps and checking its state hashes against the replay's. Neither adds
/// nor removes a character, which a replay can't hold, and a session playing a replay back takes
/// no pushes but the replay's until the replay ends, when it goes on as any other.
class Session {
  public:
    /// Creates a world from the settings, with the flat_ground scene and no characters.
    ///
    /// \param settings What the world is created from, naming no physics system of the host's.
    /// \return The session, or why its world couldn't be created.
    [[nodiscard]] static std::expected<Session, aplomb::WorldError>
    create(const aplomb::WorldSettings& settings);

    /// Creates a session that records itself as a replay: a world on flat_ground with characters
    /// made from one rig in the first places of the row, recording from before its first step, at
    /// the step rate of the settings.
    ///
    /// \param settings What the world is created from, naming no physics system of the host's.
    /// \param rig What the characters are made from.
    /// \param rig_file_hash The hash of the rig file the rig was read from.
    /// \param characters How many characters there are.
    /// \return The session, or why it couldn't be created.
    [[nodiscard]] static std::expected<Session, std::string>
    record(const aplomb::WorldSettings& settings, const aplomb::RigDesc& rig,
           std::uint64_t rig_file_hash, std::size_t characters);

    /// Creates a session that plays a replay back: a world at the replay's step rate on its scene,
    /// with its characters made from the rig where it places them.
    ///
    /// \param settings What the world is created from, naming no physics system of the host's,
    /// at any step rate.
    /// \param replay The replay.
    /// \param rig The rig its characters are made from.
    /// \param rig_file_hash The hash of the rig file the rig was read from, which must be the
    /// replay's.
    /// \return The session, or why the replay can't be played back.
    [[nodiscard]] static std::expected<Session, std::string>
    play(const aplomb::WorldSettings& settings, aplomb::ReplayDesc replay,
         const aplomb::RigDesc& rig, std::uint64_t rig_file_hash);

    /// Moves a session.
    Session(Session&& other) noexcept = default;

    /// Moves a session into this one, destroying the world this one held before its contact
    /// listener.
    Session& operator=(Session&& other) noexcept;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /// Destroys the session's world, then its contact listener.
    ~Session() = default;

    /// Adds a character made from a rig, in its bind pose, at the first free place in the row.
    /// A session that records or plays a replay back adds none.
    ///
    /// \param rig What the character is made from.
    /// \return The character's id, or why it couldn't be added.
    [[nodiscard]] std::expected<aplomb::CharacterId, aplomb::CharacterError>
    add_character(const aplomb::RigDesc& rig);

    /// Removes a character, freeing its place in the row. A session that records or plays a
    /// replay back removes none.
    ///
    /// \param character One of the session's characters.
    void remove_character(aplomb::CharacterId character);

    /// Pushes one of a character's parts with an impulse at a point, at the start of the next step.
    /// A session playing a replay back takes no other pushes.
    ///
    /// \param character One of the session's characters.
    /// \param part A part of the character's rig.
    /// \param world_point Where the impulse is applied, in the world.
    /// \param impulse The impulse, in newton seconds.
    void push(aplomb::CharacterId character, aplomb::PartId part, JPH::Vec3 world_point,
              JPH::Vec3 impulse);

    /// The session's characters, in the order they were added.
    [[nodiscard]] std::span<const aplomb::CharacterId> characters() const noexcept;

    /// Whether characters can be added or removed: neither recording nor playing a replay back.
    [[nodiscard]] bool can_change_characters() const noexcept;

    /// Steps the world by one fixed step, first sending the pushes of the replay's next step while
    /// one is played back.
    void step();

    /// How many steps the world has taken.
    [[nodiscard]] std::uint64_t steps_taken() const noexcept;

    /// The world's step rate, in steps a second.
    [[nodiscard]] float step_rate() const noexcept;

    /// The settings the world was created from, at its step rate.
    [[nodiscard]] const aplomb::WorldSettings& settings() const noexcept;

    /// Whether the session records itself as a replay.
    [[nodiscard]] bool recording() const noexcept;

    /// Stops recording.
    ///
    /// \return The replay of every step since the session started, with this build's versions of
    /// aplomb and Jolt.
    [[nodiscard]] aplomb::ReplayDesc stop_recording();

    /// How far the replay has been played back, if the session plays one.
    [[nodiscard]] const std::optional<Playback>& playback() const noexcept;

    /// Whether the session plays a replay back that hasn't ended yet.
    [[nodiscard]] bool playing() const noexcept;

    /// The session's world.
    [[nodiscard]] const aplomb::World& world() const noexcept;

    /// Every contact in the world's physics system after its last step, in no particular order.
    [[nodiscard]] std::vector<Contact> contacts() const;

  private:
    // What a session that records itself keeps from its start for its replay.
    struct ReplayStart {
        std::uint64_t rig_file_hash = 0;
        aplomb::ReplayInitialState initial{};
    };

    Session(aplomb::World world, const aplomb::WorldSettings& settings);

    // The physics system holds a pointer to the listener, so the listener outlives the world.
    std::unique_ptr<ContactTracker> contacts_;
    aplomb::World world_;
    aplomb::WorldSettings settings_;
    std::vector<aplomb::CharacterId> characters_;
    // The character at each place in the row, if any.
    std::vector<std::optional<aplomb::CharacterId>> places_;
    std::uint64_t steps_taken_ = 0;
    // Where the session's replay starts, while it records itself.
    std::optional<ReplayStart> replay_start_;
    // The replay played back, whose characters are the session's in order, while there is a
    // playback.
    aplomb::ReplayDesc replay_;
    std::optional<Playback> playback_;
};
