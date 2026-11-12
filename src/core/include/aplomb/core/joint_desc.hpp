/// \file
/// The joint between a part and its parent part.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <variant>

namespace aplomb {

/// A joint that lets its part swing within a cone and twist about an axis, as a shoulder or a hip
/// does. In the bind pose, the part has neither swung nor twisted.
///
/// Its points and axes are in the rig's space, as the parts' bind positions are. Its axes are unit
/// vectors perpendicular to each other.
struct SwingTwistJointDesc {
    /// The point the part turns about.
    JPH::Vec3 anchor = JPH::Vec3::sZero();

    /// The axis the part twists about, which points along the part, away from its parent part.
    JPH::Vec3 twist_axis = JPH::Vec3::sAxisX();

    /// The axis of the swing that `plane_half_cone` limits.
    JPH::Vec3 plane_axis = JPH::Vec3::sAxisY();

    /// How far the part may swing about the axis perpendicular to both of the others, from 0 to π
    /// radians.
    float normal_half_cone = 0.0F;

    /// How far the part may swing about the plane axis, from 0 to π radians.
    float plane_half_cone = 0.0F;

    /// The least twist, from -π to π radians and no greater than `twist_max`.
    float twist_min = 0.0F;

    /// The greatest twist, from -π to π radians and no less than `twist_min`.
    float twist_max = 0.0F;
};

/// A joint that lets its part turn about one axis, as an elbow or a knee does. In the bind pose,
/// its angle is 0.
///
/// Its points and axes are in the rig's space, as the parts' bind positions are. Its axes are unit
/// vectors perpendicular to each other.
struct HingeJointDesc {
    /// The point the part turns about.
    JPH::Vec3 anchor = JPH::Vec3::sZero();

    /// The axis the part turns about.
    JPH::Vec3 hinge_axis = JPH::Vec3::sAxisX();

    /// An axis perpendicular to the hinge axis, from which the joint's angle is measured.
    JPH::Vec3 normal_axis = JPH::Vec3::sAxisY();

    /// The least angle, from -π to 0 radians.
    float limit_min = 0.0F;

    /// The greatest angle, from 0 to π radians and greater than `limit_min`.
    float limit_max = 0.0F;
};

/// The joint between a part and its parent part.
using JointDesc = std::variant<SwingTwistJointDesc, HingeJointDesc>;

} // namespace aplomb
