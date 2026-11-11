/// \file
/// The object layers of a world's own physics system, which a host puts its bodies on.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ObjectLayer.h>

/// The object layers of the physics system that a world owns. Bodies on `scene` never collide
/// with each other; every other pair of layers collides.
namespace aplomb::object_layers {

/// The static bodies of the scene, such as the ground.
inline constexpr JPH::ObjectLayer scene = 0;

/// Every body that moves: the parts of characters, and any the host adds.
inline constexpr JPH::ObjectLayer moving = 1;

} // namespace aplomb::object_layers
