#pragma once

#include <aplomb/core/rig_problem.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <pedantjson/pedantjson.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aplomb::detail {

/// A value of a file, with its JSON pointer.
struct Field {
    /// The value.
    pedantjson::Value value;

    /// The value's JSON pointer.
    pedantjson::Pointer pointer;
};

/// A JSON file being read: its path, the document parsed from its text, and the problems found in
/// it so far, each placed at a value or a key.
class JsonFile {
  public:
    /// Starts reading a file.
    ///
    /// \param path The file's path, which each problem names.
    /// \param document The document parsed from the file's text, which must outlive this.
    JsonFile(std::string path, const pedantjson::Document& document) noexcept;

    /// The document parsed from the file's text.
    [[nodiscard]] const pedantjson::Document& document() const noexcept { return document_; }

    /// The file's root value, as a field.
    [[nodiscard]] Field root() const;

    /// Reports a problem.
    ///
    /// \param code What kind of problem it is.
    /// \param pointer The JSON pointer of the value it is with.
    /// \param offset Where in the text it is, in bytes: at a value, or at a member's key.
    /// \param message What is wrong.
    /// \param rig_problem The kind of problem the rig has, when the code is `invalid_rig`.
    void report(FileProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                std::string message, std::optional<RigProblemCode> rig_problem = std::nullopt);

    /// Reports a problem with a value, at the value.
    ///
    /// \param field The value.
    /// \param code What kind of problem it is.
    /// \param message What is wrong.
    void report_at(const Field& field, FileProblemCode code, std::string message);

    /// Whether any problem has been reported.
    [[nodiscard]] bool has_problems() const noexcept { return !problems_.empty(); }

    /// Takes the problems, in the order of their places in the file.
    [[nodiscard]] std::vector<FileProblem> take_problems() &&;

  private:
    /// A problem, and where in the text it is.
    struct PlacedProblem {
        std::size_t offset = 0;
        FileProblem problem;
    };

    std::string path_;
    std::reference_wrapper<const pedantjson::Document> document_;
    std::vector<PlacedProblem> problems_;
};

/// Parses a file's text.
///
/// \param text The text.
/// \param path The file's path, which a syntax error names.
/// \return The document, or the first place where the text stops being JSON, as a problem.
[[nodiscard]] std::expected<pedantjson::Document, FileProblem> parse_json(std::string text,
                                                                          std::string_view path);

/// Reads one object of a file key by key, reporting every problem with its keys once.
///
/// Making one reports each duplicate key. Each key is then taken by name, as required or optional,
/// and `finish` reports every key that wasn't taken.
class ObjectReader {
  public:
    /// Starts reading an object, reporting each key that an earlier key has already.
    ///
    /// \param object The object, which must be one.
    /// \param noun What the object is, such as `part`, which messages name it as.
    /// \param file The file.
    ObjectReader(const Field& object, std::string noun, JsonFile& file);

    /// Takes a key's value, reporting it missing when it is required and the object lacks it.
    ///
    /// \param name The key.
    /// \return The value of its first member, or nothing if the object lacks it.
    [[nodiscard]] std::optional<Field> take(std::string_view name);

    /// Takes a key's value, which the object may lack.
    ///
    /// \param name The key.
    /// \return The value of its first member, or nothing if the object lacks it.
    [[nodiscard]] std::optional<Field> take_optional(std::string_view name);

    /// Changes what messages name the object as, once its kind is known.
    ///
    /// \param noun What the object is, such as `box shape`.
    void rename(std::string noun) { noun_ = std::move(noun); }

    /// Reports every key that wasn't taken.
    void finish();

  private:
    [[nodiscard]] std::optional<Field> take(std::string_view name, bool required);

    pedantjson::Object object_;
    Field field_;
    std::string noun_;
    std::reference_wrapper<JsonFile> file_;
    std::vector<bool> taken_;
};

/// Reads an object.
///
/// \param field The value.
/// \param noun What the object is, which messages name it as.
/// \param file The file.
/// \return A reader of the object, or nothing if the value isn't one, which is reported.
[[nodiscard]] std::optional<ObjectReader> read_object(const Field& field, std::string noun,
                                                      JsonFile& file);

/// Reads an array's elements.
///
/// \param field The value.
/// \param file The file.
/// \return The elements, or nothing if the value isn't an array, which is reported.
[[nodiscard]] std::optional<std::vector<Field>> read_array(const Field& field, JsonFile& file);

/// Reads a string.
///
/// \param field The value.
/// \param file The file.
/// \return The string, or nothing if the value isn't one, which is reported.
[[nodiscard]] std::optional<std::string> read_string(const Field& field, JsonFile& file);

/// Reads a number as the float nearest it.
///
/// \param field The value.
/// \param file The file.
/// \return The number, or nothing if the value isn't one or no float holds it, which is reported.
[[nodiscard]] std::optional<float> read_number(const Field& field, JsonFile& file);

/// Reports a value of the wrong kind.
///
/// \param field The value.
/// \param expected The kind it should have been, with its article, such as `a number`.
/// \param file The file.
void report_wrong_type(const Field& field, std::string_view expected, JsonFile& file);

/// Reads a number as the double nearest it.
///
/// \param field The value.
/// \param file The file.
/// \return The number, or nothing if the value isn't one or no double holds it, which is reported.
[[nodiscard]] std::optional<double> read_double(const Field& field, JsonFile& file);

/// Reads a field's value into `out`, if the object has the field and its value reads. A value
/// that doesn't read has been reported, and leaves `out` as it was.
///
/// \param field The field, or nothing if the object lacks it.
/// \param out Where the value goes.
/// \param read What reads the value, such as `read_string`.
/// \param file The file.
template <typename T, typename Read>
void read_into(const std::optional<Field>& field, T& out, Read read, JsonFile& file) {
    if (field.has_value()) {
        if (auto value = read(*field, file)) {
            out = std::move(*value);
        }
    }
}

/// Reads an array of a fixed number of numbers, reporting each element that isn't a number.
///
/// \tparam Count How many numbers the array must have.
/// \param field The value.
/// \param file The file.
/// \return The numbers, or nothing if any problem was reported.
template <std::size_t Count>
[[nodiscard]] std::optional<std::array<float, Count>> read_numbers(const Field& field,
                                                                   JsonFile& file);

/// Reads an array of three numbers as a vector.
///
/// \param field The value.
/// \param file The file.
/// \return The vector, or nothing if any problem was reported.
[[nodiscard]] std::optional<JPH::Vec3> read_vector(const Field& field, JsonFile& file);

/// Reads a seed, a whole number from 0 that a `std::uint64_t` holds.
///
/// \param field The value.
/// \param file The file.
/// \return The seed, or nothing if the value isn't one, which is reported.
[[nodiscard]] std::optional<std::uint64_t> read_seed(const Field& field, JsonFile& file);

/// Reads how a character starts: an object of a `root_position` and a `pose`, which is `"bind"`.
///
/// \param field The value.
/// \param noun What the object is, such as `initial state`, which messages name it as.
/// \param file The file.
/// \return How the character starts, which is what the file gives only if no problem was
/// reported.
[[nodiscard]] InitialState read_initial_state(const Field& field, std::string noun, JsonFile& file);

/// Reads the `format` and `version` keys of a file's root object, which every file of aplomb's has.
///
/// \param root The root object.
/// \param kind What the file is, such as `rig file`, which messages name it as.
/// \param format The format the file must have, such as `aplomb.rig`.
/// \param newest_version The newest version of the format that the reader reads.
/// \param file The file.
/// \return The file's version, or nothing if either key has a problem, which is reported.
[[nodiscard]] std::optional<std::uint32_t>
read_format_and_version(ObjectReader& root, std::string_view kind, std::string_view format,
                        std::uint32_t newest_version, JsonFile& file);

/// The kind of a JSON value, as messages name it, such as `a number`.
///
/// \param kind The kind.
/// \return Its name, with its article.
[[nodiscard]] std::string_view kind_name(pedantjson::Kind kind) noexcept;

extern template std::optional<std::array<float, 3>> read_numbers<3>(const Field& field,
                                                                    JsonFile& file);
extern template std::optional<std::array<float, 4>> read_numbers<4>(const Field& field,
                                                                    JsonFile& file);

} // namespace aplomb::detail
