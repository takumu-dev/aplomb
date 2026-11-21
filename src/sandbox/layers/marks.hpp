#pragma once

#include "debug_draw/debug_draw.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

/// Draws a cross of three overlay lines, one along each axis, to mark a point.
///
/// \param draw What to draw with.
/// \param center The point.
/// \param half_size How far each line reaches either side of the point.
/// \param color The cross's colour.
void draw_cross(DebugDraw& draw, JPH::Vec3 center, float half_size, JPH::Color color);

/// Draws an arc of overlay lines around an axis, with a spoke from its centre to each of its ends.
///
/// \param draw What to draw with.
/// \param center The arc's centre.
/// \param axis The axis it turns about, as a unit vector.
/// \param start The direction from the centre at an angle of 0, a unit vector perpendicular to the
/// axis.
/// \param from The angle at which the arc starts, in radians.
/// \param to The angle at which the arc ends, in radians, no less than `from`.
/// \param radius The arc's radius.
/// \param color The arc's colour.
void draw_arc(DebugDraw& draw, JPH::Vec3 center, JPH::Vec3 axis, JPH::Vec3 start, float from,
              float to, float radius, JPH::Color color);
