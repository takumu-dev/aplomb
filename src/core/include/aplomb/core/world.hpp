/// \file
/// The world: a host's characters and the physics system they live in.

#pragma once

#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

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

    /// Advances the world by one fixed step, stepping its physics system once.
    void step();

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
