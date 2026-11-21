/// \file
/// A problem in a file, placed at the value it is with.

#pragma once

#include <aplomb/core/rig_problem.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace aplomb {

/// What kind of problem a file has.
///
/// A file is read in phases: its syntax, then its structure, then its meaning. A phase that finds
/// problems reports every one of them, and the phases after it don't run, since what they would
/// read can't be trusted.
enum class FileProblemCode : std::uint8_t {
    /// The text isn't JSON. Nothing after the first such mistake can be read, so a file has at
    /// most one.
    syntax_error,

    /// An object has a key that an earlier key of the object has already.
    duplicate_key,

    /// An object has a key that the format doesn't give it.
    unknown_key,

    /// An object lacks a key that the format requires.
    missing_key,

    /// A value is of the wrong kind, such as a string where a number belongs.
    wrong_type,

    /// An array has the wrong number of elements.
    wrong_length,

    /// A number is too large or too near zero to read.
    number_out_of_range,

    /// A value is of the right kind but none the format allows, such as an unknown shape type.
    invalid_value,

    /// The file is of a newer version of its format than this reader knows.
    unsupported_version,

    /// What the file describes is a rig with a problem, which `FileProblem::rig_problem` names.
    invalid_rig,
};

/// A problem in a file, with where it is.
struct FileProblem {
    /// What kind of problem it is, for a program to branch on.
    FileProblemCode code{};

    /// The kind of problem the rig has, when the code is `invalid_rig`.
    std::optional<RigProblemCode> rig_problem{};

    /// The file's path, as it was given.
    std::string path{};

    /// The line the problem is on, counting from 1.
    std::size_t line = 0;

    /// The column the problem is in, counting from 1 in Unicode code points.
    std::size_t column = 0;

    /// The JSON pointer of the value the problem is with, such as `/parts/2/mass`, which is empty
    /// for the whole document and for a syntax error. A key that is missing has the pointer it
    /// would have, and the line and column of the object that lacks it.
    std::string pointer{};

    /// What is wrong, for a person to read.
    std::string message{};
};

} // namespace aplomb

/// Formats a file problem as a compiler formats a diagnostic: `path:line:column: pointer: message`,
/// leaving out the pointer when it is empty.
template <> struct std::formatter<aplomb::FileProblem> : std::formatter<std::string_view> {
    /// Writes a problem, taking the format specification of a string.
    ///
    /// \param problem The problem.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const aplomb::FileProblem& problem, FormatContext& context) const {
        const std::string text =
            problem.pointer.empty() ? std::format("{}:{}:{}: {}", problem.path, problem.line,
                                                  problem.column, problem.message)
                                    : std::format("{}:{}:{}: {}: {}", problem.path, problem.line,
                                                  problem.column, problem.pointer, problem.message);
        return std::formatter<std::string_view>::format(text, context);
    }
};
