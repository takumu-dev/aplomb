/// \file
/// The world: a host's characters and the physics system they live in.

#pragma once

#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <expected>
#include <memory>

namespace aplomb {

/// A host's characters and the Jolt physics system they live in, which the world owns.
///
/// The world steps at a fixed rate. Its physics system's bodies go on the layers in
/// `aplomb::object_layers`. A moved-from world may only be destroyed or assigned to; anything
/// else fires an assertion.
class World {
  public:
    /// Creates a world that owns a physics system of its own.
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

    /// Destroys the world and its physics system, with every body in it.
    ~World();

    /// Advances the world by one fixed step, stepping its physics system once, then reads each
    /// character's physics pose.
    void step();

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
    /// the same hash at every step ran alike.
    [[nodiscard]] std::uint64_t state_hash() const;

    /// The physics system the world owns, to which the host adds the bodies of its scene.
    [[nodiscard]] JPH::PhysicsSystem& physics_system();

    /// The physics system the world owns.
    [[nodiscard]] const JPH::PhysicsSystem& physics_system() const;

  private:
    struct State;

    explicit World(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> state_;
};

} // namespace aplomb
