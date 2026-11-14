#pragma once

#include <aplomb/core/character.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <optional>
#include <vector>

/// Where a part is in the bind pose, in the rig's space, and where it is in the world now, so that
/// what a rig places in its space can be carried along with the part.
struct PartFrame {
    /// Where the part is in the bind pose, in the rig's space.
    aplomb::Transform bind{};

    /// Where the part is in the world now.
    aplomb::Transform now{};

    /// Where a point of the rig's space is now, carried along with the part.
    ///
    /// \param rig_point The point, in the rig's space.
    [[nodiscard]] JPH::Vec3 carry_point(JPH::Vec3 rig_point) const;

    /// Which way a direction of the rig's space points now, turned along with the part.
    ///
    /// \param rig_direction The direction, in the rig's space.
    [[nodiscard]] JPH::Vec3 carry_direction(JPH::Vec3 rig_direction) const;
};

/// The frame of each of a character's parts, where it is now read from its body, in the order of
/// its rig's parts.
///
/// \param world The world the character is in.
/// \param character The character.
[[nodiscard]] std::vector<PartFrame> part_frames(const aplomb::World& world,
                                                 const aplomb::Character& character);

/// The parent part of one of a character's parts.
///
/// \param character The character.
/// \param part A part of the character's rig.
/// \return The parent part, or none for the root part.
[[nodiscard]] std::optional<aplomb::PartId> parent_part(const aplomb::Character& character,
                                                        aplomb::PartId part);
