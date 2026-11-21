#pragma once

#include <aplomb/core/transform.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

/// What the sandbox draws through, in world space: lines, lines drawn over everything else, and the
/// shapes parts are made of. Only its backend knows how they reach the screen.
class DebugDraw {
  public:
    virtual ~DebugDraw() = default;

    /// Draws a line.
    ///
    /// \param from Where the line starts.
    /// \param to Where the line ends.
    /// \param color The line's colour.
    virtual void line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) = 0;

    /// Draws a line over everything else in the scene, so that it shows through the shapes in front
    /// of it, as the debug layers need to show what lies inside a part.
    ///
    /// \param from Where the line starts.
    /// \param to Where the line ends.
    /// \param color The line's colour.
    virtual void overlay_line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) = 0;

    /// Draws a solid box, centred on its transform's position and lined up with its axes.
    ///
    /// \param transform Where the box is and how it is turned.
    /// \param half_extents Half the box's size along each of its axes.
    /// \param color The box's colour.
    virtual void box(const aplomb::Transform& transform, JPH::Vec3 half_extents,
                     JPH::Color color) = 0;

    /// Draws a solid capsule, centred on its transform's position, whose cylinder runs along its Y
    /// axis.
    ///
    /// \param transform Where the capsule is and how it is turned.
    /// \param radius The radius of the cylinder and of the hemispheres that cap it.
    /// \param half_height Half the length of the cylinder between the hemispheres' centres.
    /// \param color The capsule's colour.
    virtual void capsule(const aplomb::Transform& transform, float radius, float half_height,
                         JPH::Color color) = 0;

    /// Draws a solid sphere.
    ///
    /// \param transform Where the sphere is and how it is turned.
    /// \param radius The sphere's radius.
    /// \param color The sphere's colour.
    virtual void sphere(const aplomb::Transform& transform, float radius, JPH::Color color) = 0;

  protected:
    DebugDraw() = default;
    DebugDraw(const DebugDraw&) = default;
    DebugDraw(DebugDraw&&) = default;
    DebugDraw& operator=(const DebugDraw&) = default;
    DebugDraw& operator=(DebugDraw&&) = default;
};
