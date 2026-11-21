/// \file
/// Replay files, which record a session so that it can be played back exactly.

#pragma once

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/replay_desc.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace aplomb {

/// The newest version of the replay file format, which this writer writes and this reader reads.
inline constexpr std::uint32_t replay_file_version = 1;

/// Writes the text of a replay file.
///
/// A replay file is a JSON object with the keys `format`, which is `"aplomb.replay"`, `version`,
/// `aplomb_version`, `jolt_version`, `rig_file_hash`, `step_rate`, `seed`, `scene`, `initial` and
/// `steps`, which mirror a `ReplayDesc`. `initial` is an object of the `characters`, each an
/// object of a `root_position` and a `pose`, which is `"bind"`, and the `state_hash`. Each step is
/// an object of its `messages` and its `state_hash`, so that a later version can give a step more,
/// such as an animation pose. Each message is an object with a `message`, which is `"push"`, the
/// `character` and the `part` by their indices, and the `world_point` and `impulse`.
///
/// Each floating-point number is written in the shortest form that reads back as the same bits. A
/// hash is a string of 16 lower-case hexadecimal digits, since a 64-bit integer is more than many
/// readers of JSON hold exactly.
///
/// \param replay The replay, whose numbers must be finite and whose strings must be UTF-8.
/// \return The file's text.
[[nodiscard]] std::string write_replay_file(const ReplayDesc& replay);

/// Saves a replay file, replacing any file at its path.
///
/// \param path The file's path.
/// \param replay The replay, whose numbers must be finite and whose strings must be UTF-8.
/// \return Nothing, or why the file couldn't be written.
[[nodiscard]] std::expected<void, FileError> save_replay_file(const std::filesystem::path& path,
                                                              const ReplayDesc& replay);

/// Reads a replay file.
///
/// \param path The file's path, which each problem names as it is given.
/// \return The replay the file describes, or why it couldn't be read, with every problem in the
/// file.
[[nodiscard]] std::expected<ReplayDesc, FileError>
load_replay_file(const std::filesystem::path& path);

/// Reads the text of a replay file, as `write_replay_file` describes it.
///
/// \param text The file's text.
/// \param path The file's path, which each problem names.
/// \return The replay the text describes, or every problem in it.
[[nodiscard]] std::expected<ReplayDesc, FileError> parse_replay_file(std::string text,
                                                                     std::string_view path);

} // namespace aplomb
