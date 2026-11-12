/// \file
/// A position and a rotation.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

namespace aplomb {

/// Where something is and how it is turned, relative to some frame of reference.
struct Transform {
    /// Where its origin is.
    JPH::Vec3 position = JPH::Vec3::sZero();

    /// How it is turned, as a unit quaternion.
    JPH::Quat rotation = JPH::Quat::sIdentity();
};

} // namespace aplomb
