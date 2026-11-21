#pragma once

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace aplomb::detail {

/// Reads the whole text of a file.
///
/// \param path The file's path.
/// \param kind What the file is, such as `rig file`, which the error names it as.
/// \return The file's text, or why it couldn't be read.
[[nodiscard]] std::expected<std::string, FileError>
read_text_file(const std::filesystem::path& path, std::string_view kind);

/// Writes the whole text of a file, replacing any file at its path.
///
/// \param path The file's path.
/// \param kind What the file is, such as `metrics file`, which the error names it as.
/// \param text The file's text.
/// \return Nothing, or why the file couldn't be written.
[[nodiscard]] std::expected<void, FileError>
write_text_file(const std::filesystem::path& path, std::string_view kind, std::string_view text);

/// The error of a file with problems.
///
/// \param kind What the file is, such as `rig file`, which the message names it as.
/// \param path The file's path.
/// \param problems Every problem in the file, at least one, in the order of the file.
/// \return The error, whose message names every problem.
[[nodiscard]] FileError invalid_file(std::string_view kind, std::string_view path,
                                     std::vector<FileProblem> problems);

} // namespace aplomb::detail
