/// \file
/// Why a character couldn't be added to a world.

#pragma once

#include <aplomb/core/rig_problem.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace aplomb {

/// What kind of failure stopped a character from being added to a world.
enum class CharacterErrorCode : std::uint8_t {
    /// The rig has problems, which the error lists.
    invalid_rig,

    /// The world's physics system has no room for the character's bodies.
    out_of_bodies,
};

/// Why a character couldn't be added to a world.
struct CharacterError {
    /// What kind of failure it was, for a program to branch on.
    CharacterErrorCode code{};

    /// What went wrong, for a person to read, naming every problem with the rig.
    std::string message;

    /// Every problem with the rig, when the code is `invalid_rig`, in the order they were found.
    std::vector<RigProblem> problems;
};

} // namespace aplomb

/// Formats a character error as its message.
template <> struct std::formatter<aplomb::CharacterError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const aplomb::CharacterError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
