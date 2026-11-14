#include "layers/shapes_layer.hpp"

#include "debug_draw/debug_draw.hpp"
#include "layers/part_frames.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>

#include <array>
#include <cstddef>
#include <span>
#include <variant>
#include <vector>

namespace {

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

} // namespace

void draw_shapes_layer(const aplomb::World& world, std::span<const aplomb::CharacterId> characters,
                       DebugDraw& draw) {
    const std::span<const JPH::Color> colors = character_colors;
    for (const aplomb::CharacterId id : characters) {
        const aplomb::Character& character = world.character(id);
        const JPH::Color color = colors[id.index() % colors.size()];
        const std::span<const aplomb::PartDesc> parts = character.rig().parts;
        const std::vector<PartFrame> frames = part_frames(world, character);
        for (std::size_t part = 0; part < parts.size(); ++part) {
            std::visit([&](const auto& shape) { draw_shape(shape, frames[part].now, color, draw); },
                       parts[part].shape);
        }
    }
}
