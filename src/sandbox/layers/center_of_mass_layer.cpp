#include "layers/center_of_mass_layer.hpp"

#include "debug_draw/debug_draw.hpp"
#include "layers/marks.hpp"
#include "layers/part_frames.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

#include <cstddef>
#include <span>
#include <vector>

namespace {

constexpr JPH::Color center_color(240, 90, 240);
constexpr float center_half_size = 0.08F;
constexpr float ground_half_size = 0.05F;
// The flat_ground scene's top face.
constexpr float ground_height = 0.0F;

JPH::Vec3 center_of_mass(const aplomb::World& world, const aplomb::Character& character) {
    const std::span<const aplomb::PartDesc> parts = character.rig().parts;
    const std::vector<PartFrame> frames = part_frames(world, character);
    JPH::Vec3 weighted = JPH::Vec3::sZero();
    float mass = 0.0F;
    for (std::size_t part = 0; part < parts.size(); ++part) {
        weighted += parts[part].mass * frames[part].now.position;
        mass += parts[part].mass;
    }
    // A character's rig has been checked, so every part's mass is positive.
    return weighted / mass;
}

} // namespace

void draw_center_of_mass_layer(const aplomb::World& world,
                               std::span<const aplomb::CharacterId> characters, DebugDraw& draw) {
    for (const aplomb::CharacterId id : characters) {
        const JPH::Vec3 center = center_of_mass(world, world.character(id));
        const JPH::Vec3 below(center.GetX(), ground_height, center.GetZ());
        draw_cross(draw, center, center_half_size, center_color);
        draw.overlay_line(center, below, center_color);
        draw_cross(draw, below, ground_half_size, center_color);
    }
}
