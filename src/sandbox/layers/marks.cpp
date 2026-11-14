#include "layers/marks.hpp"

#include "debug_draw/debug_draw.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cmath>

namespace {

// The longest angle a straight line of an arc stands for, so that a full turn takes 32.
constexpr float arc_step = JPH::JPH_PI / 16.0F;

} // namespace

void draw_cross(DebugDraw& draw, JPH::Vec3 center, float half_size, JPH::Color color) {
    for (const JPH::Vec3 axis : {JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY(), JPH::Vec3::sAxisZ()}) {
        draw.overlay_line(center - (half_size * axis), center + (half_size * axis), color);
    }
}

void draw_arc(DebugDraw& draw, JPH::Vec3 center, JPH::Vec3 axis, JPH::Vec3 start, float from,
              float to, float radius, JPH::Color color) {
    const auto point = [&](float angle) {
        return center + (radius * (JPH::Quat::sRotation(axis, angle) * start));
    };
    const int lines = std::max(1, static_cast<int>(std::ceil((to - from) / arc_step)));
    JPH::Vec3 last = point(from);
    draw.overlay_line(center, last, color);
    for (int line = 1; line <= lines; ++line) {
        const JPH::Vec3 next =
            point(from + ((to - from) * static_cast<float>(line) / static_cast<float>(lines)));
        draw.overlay_line(last, next, color);
        last = next;
    }
    draw.overlay_line(last, center, color);
}
