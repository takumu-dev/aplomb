#include "layers/joint_limits_layer.hpp"

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
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Trigonometry.h>
#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace {

constexpr JPH::Color swing_color(90, 210, 240);
constexpr JPH::Color twist_color(120, 140, 250);
constexpr JPH::Color hinge_color(250, 200, 60);
constexpr JPH::Color now_color(255, 90, 90);

// How long a swing cone is, and the radius of a hinge's arc.
constexpr float limit_radius = 0.1F;
// The radius of a twist range's arc, smaller than its cone so that the two don't overlap.
constexpr float twist_radius = 0.06F;
// How far past its cone or arc each line of where a part points now reaches.
constexpr float now_reach = 1.3F;

constexpr int cone_lines = 32;
constexpr int cone_spokes = 8;

// A direction of the rig's space, turned along with a part, made perpendicular to an axis.
JPH::Vec3 carry_perpendicular(const PartFrame& part, JPH::Vec3 rig_direction, JPH::Vec3 axis) {
    const JPH::Vec3 direction = part.carry_direction(rig_direction);
    return (direction - (direction.Dot(axis) * axis)).Normalized();
}

// The direction at an angle around the edge of a swing cone, whose half-angles about the plane and
// normal axes are those of Jolt's elliptical cone: a swing quaternion's parts along those axes lie
// on the ellipse whose half-axes are the sines of half of each half-angle.
JPH::Vec3 cone_edge(JPH::Vec3 twist, JPH::Vec3 plane, JPH::Vec3 normal, float plane_sine,
                    float normal_sine, float angle) {
    const float cosine = JPH::Cos(angle);
    const float sine = JPH::Sin(angle);
    const float along_plane = normal_sine * cosine;
    const float along_normal = plane_sine * sine;
    const float length = std::sqrt((along_plane * along_plane) + (along_normal * along_normal));
    const float half_swing_sine =
        length > 0.0F ? std::min(plane_sine * normal_sine / length, 1.0F) : 0.0F;
    const float swing = 2.0F * JPH::ASin(half_swing_sine);
    return JPH::Quat::sRotation((cosine * plane) + (sine * normal), swing) * twist;
}

// Draws the cone a swing-twist joint's twist axis may swing within, around its anchor. A cone with
// a half-angle of 0 about one axis is flat, an arc of the swing about the other.
void draw_swing_cone(const aplomb::SwingTwistJointDesc& joint, JPH::Vec3 anchor, JPH::Vec3 twist,
                     JPH::Vec3 plane, JPH::Vec3 normal, DebugDraw& draw) {
    if (joint.plane_half_cone <= 0.0F || joint.normal_half_cone <= 0.0F) {
        const bool about_normal = joint.plane_half_cone <= 0.0F;
        const float half_cone = about_normal ? joint.normal_half_cone : joint.plane_half_cone;
        draw_arc(draw, anchor, about_normal ? normal : plane, twist, -half_cone, half_cone,
                 limit_radius, swing_color);
        return;
    }
    const float plane_sine = JPH::Sin(0.5F * joint.plane_half_cone);
    const float normal_sine = JPH::Sin(0.5F * joint.normal_half_cone);
    const auto edge = [&](int line) {
        const float angle = 2.0F * JPH::JPH_PI * static_cast<float>(line) / cone_lines;
        return anchor +
               (limit_radius * cone_edge(twist, plane, normal, plane_sine, normal_sine, angle));
    };
    JPH::Vec3 last = edge(0);
    for (int line = 1; line <= cone_lines; ++line) {
        const JPH::Vec3 next = edge(line);
        draw.overlay_line(last, next, swing_color);
        if (line % (cone_lines / cone_spokes) == 0) {
            draw.overlay_line(anchor, next, swing_color);
        }
        last = next;
    }
}

void draw_limits(const aplomb::SwingTwistJointDesc& joint, const PartFrame& parent,
                 const PartFrame& part, DebugDraw& draw) {
    const JPH::Vec3 anchor = parent.carry_point(joint.anchor);
    const JPH::Vec3 twist = parent.carry_direction(joint.twist_axis).Normalized();
    const JPH::Vec3 plane = carry_perpendicular(parent, joint.plane_axis, twist);
    const JPH::Vec3 normal = twist.Cross(plane);
    draw_swing_cone(joint, anchor, twist, plane, normal, draw);
    draw_arc(draw, anchor, twist, plane, joint.twist_min, joint.twist_max, twist_radius,
             twist_color);

    const JPH::Vec3 twist_now = part.carry_direction(joint.twist_axis).Normalized();
    draw.overlay_line(anchor, anchor + (now_reach * limit_radius * twist_now), now_color);
    // The plane axis now, with the swing taken out, so that it lies in the twist arc's plane and
    // shows only the twist.
    const JPH::Vec3 unswung_plane = JPH::Quat::sFromTo(twist_now, twist) *
                                    carry_perpendicular(part, joint.plane_axis, twist_now);
    const JPH::Vec3 plane_now = (unswung_plane - (unswung_plane.Dot(twist) * twist)).Normalized();
    draw.overlay_line(anchor, anchor + (now_reach * twist_radius * plane_now), now_color);
}

void draw_limits(const aplomb::HingeJointDesc& joint, const PartFrame& parent,
                 const PartFrame& part, DebugDraw& draw) {
    const JPH::Vec3 anchor = parent.carry_point(joint.anchor);
    const JPH::Vec3 hinge = parent.carry_direction(joint.hinge_axis).Normalized();
    const JPH::Vec3 normal = carry_perpendicular(parent, joint.normal_axis, hinge);
    draw_arc(draw, anchor, hinge, normal, joint.limit_min, joint.limit_max, limit_radius,
             hinge_color);

    const JPH::Vec3 hinge_now = part.carry_direction(joint.hinge_axis);
    const JPH::Vec3 normal_now = carry_perpendicular(part, joint.normal_axis, hinge_now);
    draw.overlay_line(anchor, anchor + (now_reach * limit_radius * normal_now), now_color);
}

} // namespace

void draw_joint_limits_layer(const aplomb::World& world,
                             std::span<const aplomb::CharacterId> characters, DebugDraw& draw) {
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
            std::visit(
                [&](const auto& kind) {
                    draw_limits(kind, frames[parent->index()], frames[part.index()], draw);
                },
                *desc);
        }
    }
}
