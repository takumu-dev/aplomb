/// \file
/// Why a world couldn't be created.

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace aplomb {

/// What kind of failure stopped a world from being created.
enum class WorldErrorCode : std::uint8_t {
    /// The step rate wasn't a positive, finite number, or was so small that a step would last
    /// forever.
    invalid_step_rate,
};

/// Why a world couldn't be created.
struct WorldError {
    /// What kind of failure it was, for a program to branch on.
    WorldErrorCode code{};

    /// What went wrong, for a person to read.
    std::string message;
};

} // namespace aplomb

/// Formats a world error as its message.
template <> struct std::formatter<aplomb::WorldError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const aplomb::WorldError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
