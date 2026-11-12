/// \file
/// Why a file couldn't be read.

#pragma once

#include <aplomb/files/file_problem.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace aplomb {

/// What kind of failure stopped a file from being read.
enum class FileErrorCode : std::uint8_t {
    /// The file couldn't be opened or read.
    unreadable,

    /// The file has problems, which the error lists.
    invalid,
};

/// Why a file couldn't be read.
struct FileError {
    /// What kind of failure it was, for a program to branch on.
    FileErrorCode code{};

    /// What went wrong, for a person to read, naming every problem in the file.
    std::string message;

    /// Every problem in the file, when the code is `invalid`, in the order of the file.
    std::vector<FileProblem> problems;
};

} // namespace aplomb

/// Formats a file error as its message.
template <> struct std::formatter<aplomb::FileError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const aplomb::FileError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
