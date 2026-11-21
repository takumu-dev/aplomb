/// \file
/// Scenario files, read into scenarios.

#pragma once

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace aplomb {

/// The newest version of the scenario file format that this reader reads.
inline constexpr std::uint32_t scenario_file_version = 1;

/// Reads a scenario file.
///
/// \param path The file's path, which each problem names as it is given.
/// \return The scenario the file describes, or why it couldn't be read, with every problem in the
/// file.
[[nodiscard]] std::expected<ScenarioDesc, FileError>
load_scenario_file(const std::filesystem::path& path);

/// Reads the text of a scenario file.
///
/// A scenario file is a JSON object with the keys `format`, which is `"aplomb.scenario"`,
/// `version`, `name`, `rig`, `scene`, `duration_s`, `seed`, `initial`, `timeline`, `metrics` and
/// `pass`, which mirror a `ScenarioDesc`. `initial` is an object of a `root_position` and a
/// `pose`, which is `"bind"`. `metrics` lists metrics by name, and `pass` is an object that maps a
/// metric's name to an object of the values it is compared with, keyed `lt`, `le`, `eq`, `ge` or
/// `gt`. The timeline is an array of objects, each with a time `t` and a `message`; version 1
/// knows no messages, so its timelines are empty.
///
/// \param text The file's text.
/// \param path The file's path, which each problem names.
/// \return The scenario the text describes, or every problem in it.
[[nodiscard]] std::expected<ScenarioDesc, FileError> parse_scenario_file(std::string text,
                                                                         std::string_view path);

} // namespace aplomb
