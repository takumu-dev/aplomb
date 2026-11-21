/// \file
/// The settings of a part's muscle.

#pragma once

namespace aplomb {

/// The settings of the muscle at a part's joint, each a finite number no less than 0.
///
/// A rig keeps them, though nothing drives a muscle yet.
struct MuscleDesc {
    /// How hard the muscle pulls its joint toward a target, in newton metres a radian.
    float stiffness = 0.0F;

    /// How hard the muscle resists its joint's motion, in newton metre seconds a radian.
    float damping = 0.0F;

    /// The largest torque the muscle applies, in newton metres.
    float max_torque = 0.0F;
};

} // namespace aplomb
