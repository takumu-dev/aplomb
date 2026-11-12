#include "json_writing.hpp"

#include <pedantjson/pedantjson.hpp>

#include <expected>
#include <string>
#include <utility>

namespace aplomb::detail {

std::expected<std::string, pedantjson::WriteError> write_json(const pedantjson::Draft& value) {
    // A text file ends with a newline, which pedantjson leaves to its caller.
    return pedantjson::write(value, {.indent = 2}).transform([](std::string text) {
        return std::move(text) + '\n';
    });
}

} // namespace aplomb::detail
