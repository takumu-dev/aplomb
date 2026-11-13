#include "layers/shapes_layer.hpp"

#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace {

// The grid covers the ground near the origin, where characters are added, not all of it.
constexpr int grid_half_lines = 10;
constexpr float grid_spacing = 1.0F;
constexpr JPH::Color grid_color(70, 76, 86);
constexpr JPH::Color axis_color(110, 118, 130);

constexpr std::array character_colors{
    JPH::Color(222, 142, 74),  JPH::Color(92, 160, 214), JPH::Color(140, 196, 102),
    JPH::Color(206, 112, 160), JPH::Color(220, 196, 88), JPH::Color(126, 206, 196),
};

// Draws a part in the shape it has.
void draw_shape(const aplomb::BoxDesc& box, const aplomb::Transform& transform, JPH::Color color,
                DebugDraw& draw) {
    draw.box(transform, box.half_extents, color);
}

void draw_shape(const aplomb::CapsuleDesc& capsule, const aplomb::Transform& transform,
                JPH::Color color, DebugDraw& draw) {
    draw.capsule(transform, capsule.radius, capsule.half_height, color);
}

void draw_shape(const aplomb::SphereDesc& sphere, const aplomb::Transform& transform,
                JPH::Color color, DebugDraw& draw) {
    draw.sphere(transform, sphere.radius, color);
}

void draw_ground(DebugDraw& draw) {
    constexpr float extent = grid_half_lines * grid_spacing;
    for (int line = -grid_half_lines; line <= grid_half_lines; ++line) {
        const float offset = static_cast<float>(line) * grid_spacing;
        const JPH::Color color = line == 0 ? axis_color : grid_color;
        draw.line(JPH::Vec3(offset, 0.0F, -extent), JPH::Vec3(offset, 0.0F, extent), color);
        draw.line(JPH::Vec3(-extent, 0.0F, offset), JPH::Vec3(extent, 0.0F, offset), color);
    }
}

} // namespace

void draw_shapes_layer(const aplomb::World& world, std::span<const aplomb::CharacterId> characters,
                       DebugDraw& draw) {
    draw_ground(draw);

    const JPH::BodyInterface& bodies = world.physics_system().GetBodyInterface();
    const std::span<const JPH::Color> colors = character_colors;
    for (const aplomb::CharacterId id : characters) {
        const aplomb::Character& character = world.character(id);
        const JPH::Color color = colors[id.index() % colors.size()];
        const std::span<const aplomb::PartDesc> parts = character.rig().parts;
        for (std::size_t part = 0; part < parts.size(); ++part) {
            aplomb::Transform transform;
            bodies.GetPositionAndRotation(
                character.part_body(aplomb::PartId(static_cast<std::uint32_t>(part))),
                transform.position, transform.rotation);
            std::visit([&](const auto& shape) { draw_shape(shape, transform, color, draw); },
                       parts[part].shape);
        }
    }
}
