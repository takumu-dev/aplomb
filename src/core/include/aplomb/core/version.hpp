/// \file
/// The versions of aplomb and of the Jolt it is built with.

#pragma once

#include <string_view>

namespace aplomb {

/// aplomb's version, such as `0.1.0`.
[[nodiscard]] std::string_view version() noexcept;

/// The version of Jolt that aplomb is built with, such as `5.6.0`.
[[nodiscard]] std::string_view jolt_version() noexcept;

} // namespace aplomb
