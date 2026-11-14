#include "push_tool.hpp"

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "layers/marks.hpp"
#include "session.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace {

constexpr JPH::Color push_color(255, 170, 40);
constexpr float grab_half_size = 0.05F;
// The arrow's head, as a fraction of its length, and no longer than the longest head.
constexpr float head_fraction = 0.2F;
constexpr float max_head_length = 0.15F;

// Further than the camera ever is from what it looks at.
constexpr float ray_reach = 200.0F;

struct FoundPart {
    aplomb::CharacterId character;
    aplomb::PartId part;
};

// Which part of the session's characters a body is, if any.
std::optional<FoundPart> part_at(const Session& session, JPH::BodyID body) {
    for (const aplomb::CharacterId id : session.characters()) {
        const aplomb::Character& character = session.world().character(id);
        for (std::size_t index = 0; index < character.rig().parts.size(); ++index) {
            const aplomb::PartId part(static_cast<std::uint32_t>(index));
            if (character.part_body(part) == body) {
                return FoundPart{.character = id, .part = part};
            }
        }
    }
    return std::nullopt;
}

} // namespace

void PushTool::grab(const Session& session, const CameraView& view, JPH::Vec3 direction) {
    aim(view, direction);
    grab_.reset();
    const JPH::PhysicsSystem& physics = session.world().physics_system();
    const JPH::RRayCast ray{view.eye, ray_reach * direction};
    JPH::RayCastResult hit;
    if (!physics.GetNarrowPhaseQuery().CastRay(ray, hit)) {
        return;
    }
    const std::optional<FoundPart> grabbed = part_at(session, hit.mBodyID);
    if (!grabbed) {
        return;
    }
    const JPH::RMat44 body_transform = physics.GetBodyInterface().GetWorldTransform(hit.mBodyID);
    grab_ = Grab{
        .character = grabbed->character,
        .part = grabbed->part,
        .on_part = body_transform.InversedRotationTranslation() * ray.GetPointOnRay(hit.mFraction),
    };
}

void PushTool::aim(const CameraView& view, JPH::Vec3 direction) {
    view_ = view;
    direction_ = direction;
}

void PushTool::release(Session& session) {
    const std::optional<Drag> released = drag(session);
    if (grab_ && released && released->offset != JPH::Vec3::sZero()) {
        session.push(grab_->character, grab_->part, released->from,
                     impulse_per_meter_ * released->offset);
    }
    grab_.reset();
}

std::optional<JPH::Vec3> PushTool::impulse(const Session& session) const {
    const std::optional<Drag> now = drag(session);
    if (!now) {
        return std::nullopt;
    }
    return impulse_per_meter_ * now->offset;
}

void PushTool::draw(const Session& session, DebugDraw& draw) const {
    const std::optional<Drag> now = drag(session);
    if (!now) {
        return;
    }
    draw_cross(draw, now->from, grab_half_size, push_color);
    const float length = now->offset.Length();
    if (length == 0.0F) {
        return;
    }
    const JPH::Vec3 tip = now->from + now->offset;
    draw.overlay_line(now->from, tip, push_color);
    // The head's barbs lie in the plane of the drag, which faces the camera.
    const JPH::Vec3 back = now->offset / length;
    const JPH::Vec3 side = back.Cross(view_.forward()).Normalized();
    const float head = std::min(head_fraction * length, max_head_length);
    draw.overlay_line(tip, tip - (head * back) + (0.5F * head * side), push_color);
    draw.overlay_line(tip, tip - (head * back) - (0.5F * head * side), push_color);
}

float PushTool::impulse_per_meter() const noexcept { return impulse_per_meter_; }

void PushTool::set_impulse_per_meter(float impulse_per_meter) noexcept {
    impulse_per_meter_ =
        std::clamp(impulse_per_meter, min_impulse_per_meter, max_impulse_per_meter);
}

std::optional<PushTool::Drag> PushTool::drag(const Session& session) const {
    if (!grab_ || !std::ranges::contains(session.characters(), grab_->character)) {
        return std::nullopt;
    }
    const aplomb::World& world = session.world();
    const JPH::Vec3 from = world.physics_system().GetBodyInterface().GetWorldTransform(
                               world.character(grab_->character).part_body(grab_->part)) *
                           grab_->on_part;

    // The cursor's ray always meets the plane facing the camera, since the ray runs into the view.
    const JPH::Vec3 facing = view_.forward();
    const float along = (from - view_.eye).Dot(facing) / direction_.Dot(facing);
    return Drag{.from = from, .offset = view_.eye + (along * direction_) - from};
}
