#include "layers/part_frames.hpp"

#include <aplomb/core/character.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

JPH::Vec3 PartFrame::carry_point(JPH::Vec3 rig_point) const {
    const JPH::Vec3 on_part = bind.rotation.Conjugated() * (rig_point - bind.position);
    return now.position + (now.rotation * on_part);
}

JPH::Vec3 PartFrame::carry_direction(JPH::Vec3 rig_direction) const {
    return now.rotation * (bind.rotation.Conjugated() * rig_direction);
}

std::vector<PartFrame> part_frames(const aplomb::World& world, const aplomb::Character& character) {
    const JPH::BodyInterface& bodies = world.physics_system().GetBodyInterface();
    const std::span<const aplomb::PartDesc> parts = character.rig().parts;
    std::vector<PartFrame> frames(parts.size());
    for (std::size_t part = 0; part < frames.size(); ++part) {
        PartFrame& frame = frames[part];
        frame.bind = {.position = parts[part].bind_position, .rotation = parts[part].bind_rotation};
        bodies.GetPositionAndRotation(
            character.part_body(aplomb::PartId(static_cast<std::uint32_t>(part))),
            frame.now.position, frame.now.rotation);
    }
    return frames;
}

std::optional<aplomb::PartId> parent_part(const aplomb::Character& character, aplomb::PartId part) {
    const std::span<const aplomb::PartDesc> parts = character.rig().parts;
    const std::optional<std::string>& parent = parts[part.index()].parent;
    if (!parent) {
        return std::nullopt;
    }
    // A character's rig has been checked, so the parent is one of its parts.
    const auto found = std::ranges::find(parts, *parent, &aplomb::PartDesc::name);
    return aplomb::PartId(static_cast<std::uint32_t>(std::distance(parts.begin(), found)));
}
