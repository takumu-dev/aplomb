/// \file
/// A push message.

#pragma once

#include <aplomb/core/typed_id.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

namespace aplomb {

/// A push message: an impulse at a point on one of a character's parts, as `World::push` sends
/// it.
struct Push {
    /// The character pushed.
    CharacterId character{0};

    /// The part of the character's rig pushed.
    PartId part{0};

    /// Where the impulse is applied, in the world.
    JPH::Vec3 world_point = JPH::Vec3::sZero();

    /// The impulse, in newton seconds.
    JPH::Vec3 impulse = JPH::Vec3::sZero();

    /// Whether two pushes are the same push, comparing their vectors as numbers.
    friend bool operator==(const Push&, const Push&) = default;
};

} // namespace aplomb
