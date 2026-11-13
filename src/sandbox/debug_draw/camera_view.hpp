#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Math.h>
#include <Jolt/Math/Vec3.h>

/// Where the scene is seen from: a perspective view, with Y up.
struct CameraView {
    /// Where the eye is.
    JPH::Vec3 eye = JPH::Vec3::sZero();

    /// The point the eye looks at.
    JPH::Vec3 target = JPH::Vec3::sZero();

    /// The view's vertical field of view, in radians.
    float vertical_fov = 0.25F * JPH::JPH_PI;
};
