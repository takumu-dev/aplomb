#include "file_reading.hpp"

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace aplomb::detail {

namespace {

FileError unreadable(std::string_view kind, const std::filesystem::path& path,
                     std::string_view reason) {
    return {
        .code = FileErrorCode::unreadable,
        .message = std::format("the {} \"{}\" can't be read: {}", kind, path.string(), reason),
        .problems = {},
    };
}

} // namespace

std::expected<std::string, FileError> read_text_file(const std::filesystem::path& path,
                                                     std::string_view kind) {
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(path, error);
    if (!std::filesystem::exists(status)) {
        return std::unexpected(unreadable(kind, path, "there is no such file"));
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::unexpected(unreadable(kind, path, "it isn't a regular file"));
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return std::unexpected(unreadable(kind, path, "it can't be opened"));
    }
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return std::unexpected(unreadable(kind, path, "reading it failed"));
    }
    return text;
}

FileError invalid_file(std::string_view kind, std::string_view path,
                       std::vector<FileProblem> problems) {
    std::string message = std::format("the {} \"{}\" has {} problem{}: ", kind, path,
                                      problems.size(), problems.size() == 1 ? "" : "s");
    for (std::size_t index = 0; index < problems.size(); ++index) {
        message += index == 0 ? "" : "; ";
        message += std::format("{}", problems[index]);
    }
    return {
        .code = FileErrorCode::invalid,
        .message = std::move(message),
        .problems = std::move(problems),
    };
}

} // namespace aplomb::detail
