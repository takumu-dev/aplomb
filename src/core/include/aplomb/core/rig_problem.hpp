/// \file
/// A problem with a rig, which stops a character from being made from it.

#pragma once

#include <aplomb/core/typed_id.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace aplomb {

/// What kind of problem a rig has.
enum class RigProblemCode : std::uint8_t {
    /// Two or more parts have the same name.
    duplicate_part_name,

    /// A part's parent names no part of the rig.
    unknown_parent,

    /// No part is without a parent, so the rig has no root part.
    no_root_part,

    /// More than one part is without a parent, so the rig has several root parts.
    several_root_parts,

    /// Following parents from a part leads back to it.
    cycle,

    /// The root part has a joint.
    joint_on_root_part,

    /// A part other than the root part has no joint.
    missing_joint,

    /// The root part has a muscle.
    muscle_on_root_part,

    /// A part's mass isn't a positive, finite number.
    invalid_mass,

    /// A size of a part's shape isn't a positive, finite number.
    invalid_size,

    /// A part's bind position or a joint's anchor isn't finite.
    non_finite_position,

    /// A part's bind rotation isn't a unit quaternion.
    non_unit_rotation,

    /// A joint's axis isn't a unit vector.
    non_unit_axis,

    /// A joint's two axes aren't perpendicular to each other.
    axes_not_perpendicular,

    /// A joint's limit is outside its range, or its least limit is above its greatest.
    limit_out_of_range,

    /// A muscle's setting isn't a finite number no less than 0.
    invalid_muscle_setting,
};

/// A problem with a rig.
struct RigProblem {
    /// What kind of problem it is, for a program to branch on.
    RigProblemCode code{};

    /// The part the problem is with, or none when it is with the rig as a whole.
    std::optional<PartId> part;

    /// What is wrong, for a person to read.
    std::string message;
};

} // namespace aplomb

/// Formats a rig problem as its message.
template <> struct std::formatter<aplomb::RigProblem> : std::formatter<std::string_view> {
    /// Writes a problem, taking the format specification of a string.
    ///
    /// \param problem The problem.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const aplomb::RigProblem& problem, FormatContext& context) const {
        return std::formatter<std::string_view>::format(problem.message, context);
    }
};
