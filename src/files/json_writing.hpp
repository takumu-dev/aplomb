#pragma once

#include <pedantjson/pedantjson.hpp>

#include <expected>
#include <string>

namespace aplomb::detail {

/// Writes the text of one of aplomb's JSON files, laid out as each of them is: indented by two
/// spaces a level, one member or element to a line, and ending with a newline.
///
/// \param value The file's root value.
/// \return The text, or the first value in it that JSON can't hold.
[[nodiscard]] std::expected<std::string, pedantjson::WriteError>
write_json(const pedantjson::Draft& value);

} // namespace aplomb::detail
