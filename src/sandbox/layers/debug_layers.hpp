#pragma once

/// Which of the debug layers are shown, each drawn over the scene by a function of its own.
struct DebugLayers {
    /// Every part of every character, in the shape it has.
    bool shapes = true;

    /// Each joint's anchor, joined to its part and its parent part.
    bool joints = false;

    /// How far each joint may turn, around its anchor.
    bool joint_limits = false;

    /// Where bodies touch, and which way they push each other.
    bool contacts = false;

    /// Each character's centre of mass, and the point on the ground below it.
    bool center_of_mass = false;
};
