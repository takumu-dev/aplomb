#include "json_file.hpp"

#include <aplomb/core/assertion.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/files/file_problem.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace aplomb::detail {

namespace {

std::string_view article_for(std::string_view noun) {
    return noun.find_first_of("aeiou") == 0 ? "an" : "a";
}

} // namespace

void report_wrong_type(const Field& field, std::string_view expected, JsonFile& file) {
    file.report_at(field, FileProblemCode::wrong_type,
                   std::format("expected {}, not {}", expected, kind_name(field.value.kind())));
}

JsonFile::JsonFile(std::string path, const pedantjson::Document& document) noexcept
    : path_(std::move(path)), document_(document) {}

Field JsonFile::root() const { return {.value = document_.get().root(), .pointer = {}}; }

void JsonFile::report(FileProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                      std::string message, std::optional<RigProblemCode> rig_problem) {
    const pedantjson::Position position = document_.get().position(offset);
    problems_.push_back({
        .offset = offset,
        .problem =
            {
                .code = code,
                .rig_problem = rig_problem,
                .path = path_,
                .line = position.line,
                .column = position.column,
                .pointer = std::string(pointer.text()),
                .message = std::move(message),
            },
    });
}

void JsonFile::report_at(const Field& field, FileProblemCode code, std::string message) {
    report(code, field.pointer, field.value.offset(), std::move(message));
}

std::vector<FileProblem> JsonFile::take_problems() && {
    std::ranges::stable_sort(problems_, {}, &PlacedProblem::offset);
    std::vector<FileProblem> problems;
    problems.reserve(problems_.size());
    for (PlacedProblem& placed : problems_) {
        problems.push_back(std::move(placed.problem));
    }
    return problems;
}

std::expected<pedantjson::Document, FileProblem> parse_json(std::string text,
                                                            std::string_view path) {
    auto document = pedantjson::parse(std::move(text));
    if (!document.has_value()) {
        const pedantjson::SyntaxError& error = document.error();
        return std::unexpected(FileProblem{
            .code = FileProblemCode::syntax_error,
            .rig_problem = std::nullopt,
            .path = std::string(path),
            .line = error.position.line,
            .column = error.position.column,
            .pointer = {},
            .message = std::format("the text isn't JSON: {}", error.message()),
        });
    }
    return std::move(*document);
}

ObjectReader::ObjectReader(const Field& object, std::string noun, JsonFile& file)
    : object_(object.value.as_object().value_or(pedantjson::Object{})), field_(object),
      noun_(std::move(noun)), file_(file), taken_(object_.size(), false) {
    APLOMB_DEBUG_ASSERT(object.value.kind() == pedantjson::Kind::object,
                        "an object reader reads an object");
    std::map<std::string_view, std::size_t> first_offsets;
    for (const pedantjson::Member& member : object_) {
        const auto [first, inserted] =
            first_offsets.try_emplace(member.key.name(), member.key.offset());
        if (!inserted) {
            file.report(FileProblemCode::duplicate_key, field_.pointer / member.key.name(),
                        member.key.offset(),
                        std::format("the {} has the key \"{}\" already, on line {}", noun_,
                                    member.key.name(),
                                    file.document().position(first->second).line));
        }
    }
}

std::optional<Field> ObjectReader::take(std::string_view name) { return take(name, true); }

std::optional<Field> ObjectReader::take_optional(std::string_view name) {
    return take(name, false);
}

std::optional<Field> ObjectReader::take(std::string_view name, bool required) {
    std::optional<Field> first;
    for (auto&& [member, taken] : std::views::zip(object_, taken_)) {
        if (member.key.name() == name) {
            taken = true;
            if (!first.has_value()) {
                first = Field{.value = member.value, .pointer = field_.pointer / name};
            }
        }
    }
    if (!first.has_value() && required) {
        file_.get().report(FileProblemCode::missing_key, field_.pointer / name,
                           field_.value.offset(),
                           std::format("the {} has no \"{}\", which it must have", noun_, name));
    }
    return first;
}

void ObjectReader::finish() {
    for (auto&& [member, taken] : std::views::zip(object_, taken_)) {
        if (!taken) {
            file_.get().report(FileProblemCode::unknown_key, field_.pointer / member.key.name(),
                               member.key.offset(),
                               std::format("{} {} has no key \"{}\"", article_for(noun_), noun_,
                                           member.key.name()));
            // Its duplicates are reported as duplicates already.
            std::ignore = take_optional(member.key.name());
        }
    }
}

std::optional<ObjectReader> read_object(const Field& field, std::string noun, JsonFile& file) {
    if (field.value.kind() != pedantjson::Kind::object) {
        report_wrong_type(field, "an object", file);
        return std::nullopt;
    }
    return ObjectReader(field, std::move(noun), file);
}

std::optional<std::vector<Field>> read_array(const Field& field, JsonFile& file) {
    const std::optional<pedantjson::Array> array = field.value.as_array();
    if (!array.has_value()) {
        report_wrong_type(field, "an array", file);
        return std::nullopt;
    }
    std::vector<Field> elements;
    elements.reserve(array->size());
    for (std::size_t index = 0; const pedantjson::Value element : *array) {
        elements.push_back({.value = element, .pointer = field.pointer / index++});
    }
    return elements;
}

std::optional<std::string> read_string(const Field& field, JsonFile& file) {
    const std::optional<std::string_view> string = field.value.as_string();
    if (!string.has_value()) {
        report_wrong_type(field, "a string", file);
        return std::nullopt;
    }
    return std::string(*string);
}

namespace {

// Reads a number as the floating-point value of type T nearest it.
template <typename T> std::optional<T> read_floating(const Field& field, JsonFile& file) {
    const std::optional<pedantjson::Number> number = field.value.as_number();
    if (!number.has_value()) {
        report_wrong_type(field, "a number", file);
        return std::nullopt;
    }
    // A floating-point type holds a number of any form, so being out of its range is the only way
    // this fails.
    const std::expected<T, pedantjson::NumberError> value = number->to<T>();
    if (!value.has_value()) {
        file.report_at(
            field, FileProblemCode::number_out_of_range,
            std::format("the number {} is too large or too near zero to read", number->text()));
        return std::nullopt;
    }
    return *value;
}

} // namespace

std::optional<float> read_number(const Field& field, JsonFile& file) {
    return read_floating<float>(field, file);
}

std::optional<double> read_double(const Field& field, JsonFile& file) {
    return read_floating<double>(field, file);
}

template <std::size_t Count>
std::optional<std::array<float, Count>> read_numbers(const Field& field, JsonFile& file) {
    const std::optional<std::vector<Field>> elements = read_array(field, file);
    if (!elements.has_value()) {
        return std::nullopt;
    }
    if (elements->size() != Count) {
        file.report_at(field, FileProblemCode::wrong_length,
                       std::format("expected {} numbers, not {}", Count, elements->size()));
        return std::nullopt;
    }
    std::array<float, Count> numbers{};
    bool all_read = true;
    for (auto&& [number, element] : std::views::zip(numbers, *elements)) {
        const std::optional<float> value = read_number(element, file);
        all_read = all_read && value.has_value();
        number = value.value_or(0.0F);
    }
    return all_read ? std::optional(numbers) : std::nullopt;
}

template std::optional<std::array<float, 3>> read_numbers<3>(const Field& field, JsonFile& file);
template std::optional<std::array<float, 4>> read_numbers<4>(const Field& field, JsonFile& file);

std::optional<JPH::Vec3> read_vector(const Field& field, JsonFile& file) {
    return read_numbers<3>(field, file).transform([](const std::array<float, 3>& numbers) {
        return JPH::Vec3(numbers[0], numbers[1], numbers[2]);
    });
}

std::optional<std::uint32_t> read_format_and_version(ObjectReader& root, std::string_view kind,
                                                     std::string_view format,
                                                     std::uint32_t newest_version, JsonFile& file) {
    bool format_read = false;
    if (const std::optional<Field> field = root.take("format")) {
        const std::optional<std::string> value = read_string(*field, file);
        if (value.has_value() && *value != format) {
            file.report_at(
                *field, FileProblemCode::invalid_value,
                std::format(R"(the format of a {} is "{}", not "{}")", kind, format, *value));
        }
        format_read = value == format;
    }

    std::optional<std::uint32_t> version;
    if (const std::optional<Field> field = root.take("version")) {
        if (const std::optional<pedantjson::Number> number = field->value.as_number()) {
            const auto value = number->to<std::uint32_t>();
            const bool is_newer = value.has_value()
                                      ? *value > newest_version
                                      : number->is_integer() && !number->text().starts_with('-');
            if (is_newer) {
                file.report_at(*field, FileProblemCode::unsupported_version,
                               std::format("the {} is of version {}, newer than version {}, the "
                                           "newest this reader knows",
                                           kind, number->text(), newest_version));
            } else if (!value.has_value() || *value == 0) {
                file.report_at(*field, FileProblemCode::invalid_value,
                               std::format("the version must be a whole number from 1, not {}",
                                           number->text()));
            } else {
                version = *value;
            }
        } else {
            report_wrong_type(*field, "a number", file);
        }
    }
    return format_read ? version : std::nullopt;
}

std::string_view kind_name(pedantjson::Kind kind) noexcept {
    switch (kind) {
    case pedantjson::Kind::null:
        return "null";
    case pedantjson::Kind::boolean:
        return "a boolean";
    case pedantjson::Kind::number:
        return "a number";
    case pedantjson::Kind::string:
        return "a string";
    case pedantjson::Kind::array:
        return "an array";
    case pedantjson::Kind::object:
        return "an object";
    }
    // Only a kind cast from outside the enumeration gets here.
    return "a value";
}

} // namespace aplomb::detail
