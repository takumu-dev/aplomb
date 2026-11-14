#pragma once

#include "debug_draw/camera_view.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

/// A camera that circles a target point, looking at it from any side: from any heading, from
/// straight above to straight below, and from near or far.
class OrbitCamera {
  public:
    /// Turns the camera around its target.
    ///
    /// \param heading How far to turn it around the vertical axis, in radians.
    /// \param elevation How far to raise it, in radians, stopping short of straight above or below.
    void orbit(float heading, float elevation);

    /// Moves the camera and its target together, across the view.
    ///
    /// \param right How far to move them to the right of the view, as a fraction of the distance
    /// from the camera to its target.
    /// \param up How far to move them up the view, as a fraction of that distance.
    void pan(float right, float up);

    /// Moves the camera towards or away from its target.
    ///
    /// \param steps How many steps to move it in, each a tenth nearer, or out when negative.
    void zoom(float steps);

    /// Where the camera is and what it looks at.
    [[nodiscard]] CameraView view() const;

  private:
    // From the camera to its target, as a unit vector.
    [[nodiscard]] JPH::Vec3 forward() const;

    JPH::Vec3 target_{0.0F, 0.8F, 0.0F};
    float distance_ = 5.0F;
    float heading_ = 0.6F;
    float elevation_ = 0.3F;
};
