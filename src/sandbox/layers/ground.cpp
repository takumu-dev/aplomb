#include "layers/ground.hpp"

#include "debug_draw/debug_draw.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

namespace {

// The grid covers the ground near the origin, where characters are added, not all of it.
constexpr int grid_half_lines = 10;
constexpr float grid_spacing = 1.0F;
constexpr JPH::Color grid_color(70, 76, 86);
constexpr JPH::Color axis_color(110, 118, 130);

} // namespace

void draw_ground(DebugDraw& draw) {
    constexpr float extent = grid_half_lines * grid_spacing;
    for (int line = -grid_half_lines; line <= grid_half_lines; ++line) {
        const float offset = static_cast<float>(line) * grid_spacing;
        const JPH::Color color = line == 0 ? axis_color : grid_color;
        draw.line(JPH::Vec3(offset, 0.0F, -extent), JPH::Vec3(offset, 0.0F, extent), color);
        draw.line(JPH::Vec3(-extent, 0.0F, offset), JPH::Vec3(extent, 0.0F, offset), color);
    }
}
