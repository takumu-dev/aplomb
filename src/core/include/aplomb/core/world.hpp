/// \file
/// The world: a host's characters and the physics system they live in.

#pragma once

#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <vector>

namespace aplomb {

/// A host's characters and the Jolt physics system they live in, which the world either owns or
/// shares with the host.
///
/// The world steps at a fixed rate. A world that owns its physics system steps it in `step()`, and
/// that physics system's bodies go on the layers in `aplomb::object_layers`. A world attached to
/// the host's physics system never steps it: the host calls `before_physics_step()`, updates its
/// physics system once by the world's step duration, then calls `after_physics_step()`, and the
/// world's characters' parts go on the object layer the host chose for them. The world listens to
/// the steps of the host's physics system to check them, so Jolt no longer skips an update in which
/// no body is active.
///
/// State hashes and recording, and so replays, are offered only by a world that owns its physics
/// system. An attached world's state includes the host's bodies, which the world neither creates
/// nor orders.
///
/// A moved-from world may only be destroyed or assigned to; anything else fires an assertion.
class World {
  public:
    /// Creates a world that owns a physics system of its own, or one attached to the host's physics
    /// system when the settings name it.
    ///
    /// Jolt's process-wide setup is the host's to do first: its allocator, its factory and its
    /// registered types.
    ///
    /// \param settings What the world is created from.
    /// \return The world, or why it couldn't be created.
    [[nodiscard]] static std::expected<World, WorldError> create(const WorldSettings& settings);

    /// Moves a world, leaving `other` empty.
    World(World&& other) noexcept;

    /// Moves a world into this one, destroying what this one held.
    World& operator=(World&& other) noexcept;

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// Destroys the world with its characters, taking their bodies and constraints out of the
    /// physics system. A world that owns its physics system destroys it too, with every body in
    /// it.
    ~World();

    /// Advances a world that owns its physics system by one fixed step: applies the pushes sent
    /// since the last step, in the order they were sent, steps its physics system once, then reads
    /// each character's physics pose. A recording world then records the step.
    void step();

    /// Begins a step of an attached world, before the host updates its physics system: applies
    /// the pushes sent since the last step, in the order they were sent.
    ///
    /// The host updates its physics system only between this and `after_physics_step()`.
    void before_physics_step();

    /// Ends a step of an attached world, after the host has updated its physics system: reads each
    /// character's physics pose.
    ///
    /// Since the last `before_physics_step()`, the host has updated its physics system once by the
    /// world's step duration, `1 / step_rate`, which it may split into collision steps. A host that
    /// stepped at another rate fires an assertion. Whether the update ran out of room is the
    /// host's to read from its result.
    void after_physics_step();

    /// Starts recording each step the world takes, with the pushes it applied and its state hash
    /// after it, discarding any steps recorded before. Only a world that owns its physics system
    /// records.
    void start_recording();

    /// Stops recording.
    ///
    /// \return The steps recorded since recording started, in order, or none if the world wasn't
    /// recording.
    [[nodiscard]] std::vector<RecordedStep> stop_recording();

    /// Adds a character made from a rig, in the rig's bind pose, as a passive ragdoll.
    ///
    /// The rig is checked in full first, and every problem with it is reported.
    ///
    /// \param rig What the character is made from.
    /// \param root Where the rig's space is placed in the world. Its rotation is a unit
    /// quaternion.
    /// \return The character's id, or why it couldn't be added.
    [[nodiscard]] std::expected<CharacterId, CharacterError> add_character(const RigDesc& rig,
                                                                           const Transform& root);

    /// Removes a character, taking its bodies and constraints out of the physics system.
    ///
    /// \param character A character of this world.
    void remove_character(CharacterId character);

    /// Pushes one of a character's parts with an impulse at a point, by a push message.
    ///
    /// The push takes effect at the start of the next step, after every push sent before it, so
    /// that the same pushes sent in the same order always give the same state. A push to a
    /// character that is removed before then is dropped.
    ///
    /// \param character A character of this world.
    /// \param part A part of the character's rig.
    /// \param world_point Where the impulse is applied, in the world.
    /// \param impulse The impulse, in newton seconds.
    void push(CharacterId character, PartId part, JPH::Vec3 world_point, JPH::Vec3 impulse);

    /// One of the world's characters. The reference holds until the character is removed or the
    /// world is destroyed.
    ///
    /// \param character A character of this world.
    [[nodiscard]] const Character& character(CharacterId character) const;

    /// A hash of the world's whole physical state: every body and constraint in its physics system,
    /// the scene's included, with the contacts Jolt carries from one step to the next.
    ///
    /// Jolt's state recorder saves the state in a fixed order, and the hash is the 64-bit FNV-1a of
    /// what it saves, so the same state gives the same hash on every platform. Two runs that give
    /// the same hash at every step ran alike. Only a world that owns its physics system offers a
    /// state hash.
    [[nodiscard]] std::uint64_t state_hash() const;

    /// The physics system the world's characters live in: the one the world owns, to which the
    /// host adds the bodies of its scene, or the host's own for an attached world.
    [[nodiscard]] JPH::PhysicsSystem& physics_system();

    /// The physics system the world's characters live in.
    [[nodiscard]] const JPH::PhysicsSystem& physics_system() const;

  private:
    struct State;

    // Applies the pushes sent since the last step, in the order they were sent, and returns the
    // step to record, holding the pushes that were applied.
    RecordedStep apply_pushes();

    // Reads each character's physics pose after its physics system has stepped, and records the
    // step if the world is recording.
    void finish_step(RecordedStep recorded);

    explicit World(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> state_;
};

} // namespace aplomb
