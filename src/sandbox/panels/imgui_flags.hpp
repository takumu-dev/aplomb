#pragma once

/// Combines ImGui flags, which are a plain enumeration's and so signed, through their unsigned
/// values.
///
/// \tparam Flags The flags' types.
/// \param flags The flags.
/// \return The flags combined, as ImGui takes them.
template <typename... Flags> constexpr int imgui_flags(Flags... flags) noexcept {
    return static_cast<int>((0U | ... | static_cast<unsigned>(flags)));
}
