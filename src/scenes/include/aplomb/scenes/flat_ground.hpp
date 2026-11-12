/// \file
/// The flat_ground scene: a plane of ground for characters to stand and fall on.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace aplomb {

/// Builds the flat_ground scene into a physics system: one static box with high friction, whose
/// top face is level at a height of 0 and reaches 100 m from the origin each way.
///
/// The box goes on `aplomb::object_layers::scene`, so the physics system is one a world owns.
///
/// \param physics_system The physics system to add the ground to.
/// \return The ground's body.
JPH::BodyID build_flat_ground(JPH::PhysicsSystem& physics_system);

} // namespace aplomb
