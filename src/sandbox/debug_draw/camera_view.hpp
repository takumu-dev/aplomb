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

    /// The way the eye looks, as a unit vector.
    [[nodiscard]] JPH::Vec3 forward() const;

    /// The way from the eye through a point of the view, as a unit vector.
    ///
    /// \param right How far right the point is, from -1 at the view's left edge to 1 at its right.
    /// \param up How far up the point is, from -1 at the view's bottom edge to 1 at its top.
    /// \param aspect The view's width divided by its height.
    [[nodiscard]] JPH::Vec3 direction_through(float right, float up, float aspect) const;
};
