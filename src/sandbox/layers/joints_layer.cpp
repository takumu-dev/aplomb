#include "layers/joints_layer.hpp"

#include "debug_draw/debug_draw.hpp"
#include "layers/marks.hpp"
#include "layers/part_frames.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace {

constexpr JPH::Color link_color(235, 235, 235);
constexpr JPH::Color swing_twist_color(90, 210, 240);
constexpr JPH::Color hinge_color(250, 200, 60);
constexpr float anchor_half_size = 0.03F;

} // namespace

void draw_joints_layer(const aplomb::World& world, std::span<const aplomb::CharacterId> characters,
                       DebugDraw& draw) {
    for (const aplomb::CharacterId id : characters) {
        const aplomb::Character& character = world.character(id);
        const std::span<const aplomb::PartDesc> parts = character.rig().parts;
        const std::vector<PartFrame> frames = part_frames(world, character);
        for (std::size_t joint = 0; joint < character.joint_count(); ++joint) {
            const aplomb::PartId part =
                character.joint_part(aplomb::JointId(static_cast<std::uint32_t>(joint)));
            const std::optional<aplomb::PartId> parent = parent_part(character, part);
            const std::optional<aplomb::JointDesc>& desc = parts[part.index()].joint;
            // Every part a joint links has a parent part and a joint.
            if (!parent || !desc) {
                continue;
            }
            const JPH::Vec3 anchor =
                std::visit([](const auto& kind) { return kind.anchor; }, *desc);

            const PartFrame& parent_frame = frames[parent->index()];
            const PartFrame& part_frame = frames[part.index()];
            const JPH::Vec3 on_parent = parent_frame.carry_point(anchor);
            draw.overlay_line(parent_frame.now.position, on_parent, link_color);
            draw.overlay_line(part_frame.carry_point(anchor), part_frame.now.position, link_color);
            draw_cross(draw, on_parent, anchor_half_size,
                       std::holds_alternative<aplomb::HingeJointDesc>(*desc) ? hinge_color
                                                                             : swing_twist_color);
        }
    }
}
