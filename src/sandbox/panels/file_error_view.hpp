#pragma once

#include <aplomb/files/file_error.hpp>

#include <string_view>

/// Draws why a file couldn't be read into the current ImGui window: every problem in it, each at
/// its line and column, or the message of a file that couldn't be read at all.
///
/// \param error Why the file couldn't be read.
/// \param kind What kind of file it is, such as "rig file".
void draw_file_error(const aplomb::FileError& error, std::string_view kind);
