/// \file
/// One part of a rig, as a host describes it.

#pragma once

#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/shape_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

#include <optional>
#include <string>

namespace aplomb {

/// One part of a rig: a rigid body, and the joint and muscle that link it to its parent part.
///
/// Its bind position and rotation place it in the rig's space, in the pose the rig is built in.
struct PartDesc {
    /// The part's name, which no other part of the rig has.
    std::string name{};

    /// The name of the part's parent part, or none for the root part.
    std::optional<std::string> parent{};

    /// The part's shape.
    ShapeDesc shape = BoxDesc{};

    /// The part's mass, a positive number of kilograms.
    float mass = 0.0F;

    /// Where the part's origin is in the bind pose.
    JPH::Vec3 bind_position = JPH::Vec3::sZero();

    /// How the part is turned in the bind pose, as a unit quaternion.
    JPH::Quat bind_rotation = JPH::Quat::sIdentity();

    /// The joint that links the part to its parent part, which every part but the root part has.
    std::optional<JointDesc> joint{};

    /// The settings of the muscle at the part's joint, which the root part never has.
    std::optional<MuscleDesc> muscle{};
};

} // namespace aplomb
