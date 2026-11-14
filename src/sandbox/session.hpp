#pragma once

#include "contact_tracker.hpp"
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

/// What the sandbox shows: a world that owns its physics system, built on the flat_ground scene,
/// and the characters the owner adds onto it, with a contact listener on its physics system that
/// keeps the contacts there.
///
/// Each character is dropped from just above the ground at a place of its own in a row along the X
/// axis, the place nearest the origin that no other character holds.
class Session {
  public:
    /// Creates a world from the settings, with the flat_ground scene and no characters.
    ///
    /// \param settings What the world is created from, naming no physics system of the host's.
    /// \return The session, or why its world couldn't be created.
    [[nodiscard]] static std::expected<Session, aplomb::WorldError>
    create(const aplomb::WorldSettings& settings);

    /// Adds a character made from a rig, in its bind pose, at the first free place in the row.
    ///
    /// \param rig What the character is made from.
    /// \return The character's id, or why it couldn't be added.
    [[nodiscard]] std::expected<aplomb::CharacterId, aplomb::CharacterError>
    add_character(const aplomb::RigDesc& rig);

    /// Removes a character, freeing its place in the row.
    ///
    /// \param character One of the session's characters.
    void remove_character(aplomb::CharacterId character);

    /// Pushes one of a character's parts with an impulse at a point, at the start of the next step.
    ///
    /// \param character One of the session's characters.
    /// \param part A part of the character's rig.
    /// \param world_point Where the impulse is applied, in the world.
    /// \param impulse The impulse, in newton seconds.
    void push(aplomb::CharacterId character, aplomb::PartId part, JPH::Vec3 world_point,
              JPH::Vec3 impulse);

    /// The session's characters, in the order they were added.
    [[nodiscard]] std::span<const aplomb::CharacterId> characters() const noexcept;

    /// Steps the world by one fixed step.
    void step();

    /// How many steps the world has taken.
    [[nodiscard]] std::uint64_t steps_taken() const noexcept;

    /// The world's step rate, in steps a second.
    [[nodiscard]] float step_rate() const noexcept;

    /// The session's world.
    [[nodiscard]] const aplomb::World& world() const noexcept;

    /// Every contact in the world's physics system after its last step, in no particular order.
    [[nodiscard]] std::vector<Contact> contacts() const;

  private:
    Session(aplomb::World world, std::unique_ptr<ContactTracker> contacts,
            float step_rate) noexcept;

    // The physics system holds a pointer to the listener, so the listener outlives the world.
    std::unique_ptr<ContactTracker> contacts_;
    aplomb::World world_;
    float step_rate_;
    std::vector<aplomb::CharacterId> characters_;
    // The character at each place in the row, if any.
    std::vector<std::optional<aplomb::CharacterId>> places_;
    std::uint64_t steps_taken_ = 0;
};
