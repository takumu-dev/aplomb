/// \file
/// The shape of a part.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <variant>

namespace aplomb {

/// A box, centred on its part's origin and lined up with its part's axes.
struct BoxDesc {
    /// Half the box's size along each of its part's axes, in metres.
    JPH::Vec3 half_extents = JPH::Vec3::sZero();
};

/// A capsule, centred on its part's origin, whose cylinder runs along its part's Y axis.
struct CapsuleDesc {
    /// The radius of the cylinder and of the hemispheres that cap it, in metres.
    float radius = 0.0F;

    /// Half the length of the cylinder between the hemispheres' centres, in metres.
    float half_height = 0.0F;
};

/// A sphere, centred on its part's origin.
struct SphereDesc {
    /// The sphere's radius, in metres.
    float radius = 0.0F;
};

/// The shape of a part, every size of which is positive.
using ShapeDesc = std::variant<BoxDesc, CapsuleDesc, SphereDesc>;

} // namespace aplomb
