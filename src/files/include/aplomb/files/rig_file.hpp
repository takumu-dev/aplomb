/// \file
/// Rig files, read into rigs.

#pragma once

#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace aplomb {

/// The newest version of the rig file format that this reader reads.
inline constexpr std::uint32_t rig_file_version = 1;

/// Reads a rig file.
///
/// \param path The file's path, which each problem names as it is given.
/// \return The rig the file describes, or why it couldn't be read, with every problem in the file.
[[nodiscard]] std::expected<RigDesc, FileError> load_rig_file(const std::filesystem::path& path);

/// Reads the text of a rig file.
///
/// A rig file is a JSON object with the keys `format`, which is `"aplomb.rig"`, `version`, `name`
/// and `parts`, each part an object that mirrors a `PartDesc`. Lengths are in metres, masses in
/// kilograms and angles in degrees, which the rig gets in radians. A muscle's settings are per
/// radian, as a `MuscleDesc`'s are, since they aren't angles themselves.
///
/// \param text The file's text.
/// \param path The file's path, which each problem names.
/// \return The rig the text describes, or every problem in it.
[[nodiscard]] std::expected<RigDesc, FileError> parse_rig_file(std::string text,
                                                               std::string_view path);

/// The hash of a rig file's text, by which a replay names the rig file it was recorded with.
///
/// It is the 64-bit FNV-1a of the text's bytes, so the same text gives the same hash on every
/// platform, and a change of a single byte, a line ending's included, gives another.
///
/// \param text The file's text.
/// \return The hash.
[[nodiscard]] std::uint64_t rig_file_hash(std::string_view text) noexcept;

/// Reads the hash of a rig file, as `rig_file_hash` gives it of the file's text.
///
/// \param path The file's path.
/// \return The hash, or why the file couldn't be read.
[[nodiscard]] std::expected<std::uint64_t, FileError>
load_rig_file_hash(const std::filesystem::path& path);

} // namespace aplomb
