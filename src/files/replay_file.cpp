#include <aplomb/files/replay_file.hpp>

#include "file_reading.hpp"
#include "json_file.hpp"
#include "json_writing.hpp"
#include <aplomb/core/assertion.hpp>
#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace aplomb {

namespace {

using detail::Field;
using detail::JsonFile;

constexpr std::string_view kind = "replay file";

pedantjson::Draft hash_text(std::uint64_t hash) { return std::format("{:016x}", hash); }

pedantjson::Draft vector(JPH::Vec3 vector) {
    pedantjson::Draft numbers = pedantjson::Draft::array();
    numbers.append(vector.GetX()).append(vector.GetY()).append(vector.GetZ());
    return numbers;
}

pedantjson::Draft initial_state(const ReplayInitialState& initial) {
    pedantjson::Draft characters = pedantjson::Draft::array();
    for (const InitialState& character : initial.characters) {
        pedantjson::Draft object = pedantjson::Draft::object();
        object.append("root_position", vector(character.root_position)).append("pose", "bind");
        characters.append(std::move(object));
    }
    pedantjson::Draft object = pedantjson::Draft::object();
    object.append("characters", std::move(characters))
        .append("state_hash", hash_text(initial.state_hash));
    return object;
}

pedantjson::Draft step(const RecordedStep& step) {
    pedantjson::Draft messages = pedantjson::Draft::array();
    for (const Push& push : step.pushes) {
        pedantjson::Draft message = pedantjson::Draft::object();
        message.append("message", "push")
            .append("character", push.character.index())
            .append("part", push.part.index())
            .append("world_point", vector(push.world_point))
            .append("impulse", vector(push.impulse));
        messages.append(std::move(message));
    }
    pedantjson::Draft object = pedantjson::Draft::object();
    object.append("messages", std::move(messages)).append("state_hash", hash_text(step.state_hash));
    return object;
}

// A hash is exactly 16 lower-case hexadecimal digits, as the writer writes it.
std::optional<std::uint64_t> read_hash(const Field& field, JsonFile& file) {
    const std::optional<std::string> text = detail::read_string(field, file);
    if (!text.has_value()) {
        return std::nullopt;
    }
    constexpr std::size_t digits = 16;
    constexpr int base = 16;
    std::uint64_t hash = 0;
    const char* const text_end = std::to_address(text->end());
    const auto [end, error] = std::from_chars(text->data(), text_end, hash, base);
    if (text->size() != digits || error != std::errc() || end != text_end ||
        std::ranges::any_of(*text, [](char digit) { return digit >= 'A' && digit <= 'F'; })) {
        file.report_at(
            field, FileProblemCode::invalid_value,
            std::format(R"(a hash is 16 lower-case hexadecimal digits, not "{}")", *text));
        return std::nullopt;
    }
    return hash;
}

// An index into a table, such as a character's into the replay's characters.
template <typename Id>
std::optional<Id> read_index(const Field& field, std::string_view noun, JsonFile& file) {
    const std::optional<pedantjson::Number> number = field.value.as_number();
    if (!number.has_value()) {
        detail::report_wrong_type(field, "a number", file);
        return std::nullopt;
    }
    const auto index = number->to<std::uint32_t>();
    if (!index.has_value()) {
        file.report_at(field, FileProblemCode::invalid_value,
                       std::format("a {} is given by its index, a whole number from 0, not {}",
                                   noun, number->text()));
        return std::nullopt;
    }
    return Id(*index);
}

std::optional<CharacterId> read_character(const Field& field, JsonFile& file) {
    return read_index<CharacterId>(field, "character", file);
}

std::optional<PartId> read_part(const Field& field, JsonFile& file) {
    return read_index<PartId>(field, "part", file);
}

// Reads the structure of a replay file into a replay. Each part of the reader fills in what it can
// and reports what it can't, so that one pass finds every problem; the replay is used only if none
// was reported.
class ReplayFileReader {
  public:
    explicit ReplayFileReader(JsonFile& file) : file_(file) {}

    ReplayDesc read() {
        ReplayDesc replay;
        std::optional<detail::ObjectReader> root =
            detail::read_object(file_.get().root(), std::string(kind), file_);
        if (!root.has_value()) {
            return replay;
        }
        // A file of another format or of a newer version can't be read any further, since its
        // other keys can't be known.
        if (!detail::read_format_and_version(*root, kind, "aplomb.replay", replay_file_version,
                                             file_)
                 .has_value()) {
            return replay;
        }
        detail::read_into(root->take("aplomb_version"), replay.aplomb_version, detail::read_string,
                          file_);
        detail::read_into(root->take("jolt_version"), replay.jolt_version, detail::read_string,
                          file_);
        detail::read_into(root->take("rig_file_hash"), replay.rig_file_hash, read_hash, file_);
        detail::read_into(root->take("step_rate"), replay.step_rate, detail::read_number, file_);
        detail::read_into(root->take("seed"), replay.seed, detail::read_seed, file_);
        detail::read_into(root->take("scene"), replay.scene, detail::read_string, file_);
        if (const std::optional<Field> initial = root->take("initial")) {
            replay.initial = read_initial(*initial);
        }
        if (const std::optional<Field> steps = root->take("steps")) {
            for (const Field& step : detail::read_array(*steps, file_).value_or({})) {
                replay.steps.push_back(read_step(step));
            }
        }
        root->finish();
        return replay;
    }

  private:
    ReplayInitialState read_initial(const Field& field) {
        ReplayInitialState initial;
        std::optional<detail::ObjectReader> reader =
            detail::read_object(field, "initial state", file_);
        if (!reader.has_value()) {
            return initial;
        }
        if (const std::optional<Field> characters = reader->take("characters")) {
            for (const Field& character : detail::read_array(*characters, file_).value_or({})) {
                initial.characters.push_back(
                    detail::read_initial_state(character, "character", file_));
            }
        }
        detail::read_into(reader->take("state_hash"), initial.state_hash, read_hash, file_);
        reader->finish();
        return initial;
    }

    RecordedStep read_step(const Field& field) {
        RecordedStep step;
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "step", file_);
        if (!reader.has_value()) {
            return step;
        }
        if (const std::optional<Field> messages = reader->take("messages")) {
            for (const Field& message : detail::read_array(*messages, file_).value_or({})) {
                if (std::optional<Push> push = read_message(message)) {
                    step.pushes.push_back(*push);
                }
            }
        }
        detail::read_into(reader->take("state_hash"), step.state_hash, read_hash, file_);
        reader->finish();
        return step;
    }

    // Version 1 knows one message, a push. A message that isn't known can't have its other keys
    // checked, so its reader doesn't finish.
    std::optional<Push> read_message(const Field& field) {
        std::optional<detail::ObjectReader> reader = detail::read_object(field, "message", file_);
        if (!reader.has_value()) {
            return std::nullopt;
        }
        const std::optional<Field> message = reader->take("message");
        if (!message.has_value()) {
            return std::nullopt;
        }
        const std::optional<std::string> name = detail::read_string(*message, file_);
        if (name != "push") {
            if (name.has_value()) {
                file_.get().report_at(
                    *message, FileProblemCode::invalid_value,
                    std::format(R"(the message of a step is "push", not "{}")", *name));
            }
            return std::nullopt;
        }
        reader->rename("push");
        Push push;
        detail::read_into(reader->take("character"), push.character, read_character, file_);
        detail::read_into(reader->take("part"), push.part, read_part, file_);
        detail::read_into(reader->take("world_point"), push.world_point, detail::read_vector,
                          file_);
        detail::read_into(reader->take("impulse"), push.impulse, detail::read_vector, file_);
        reader->finish();
        return push;
    }

    std::reference_wrapper<JsonFile> file_;
};

// Reports what is wrong with what a replay file describes, once its structure has been read.
void check_meaning(const ReplayDesc& replay, const pedantjson::Document& document, JsonFile& file) {
    const auto report = [&](const pedantjson::Pointer& pointer, std::string message) {
        file.report(FileProblemCode::invalid_value, pointer, document.nearest(pointer).offset(),
                    std::move(message));
    };
    const pedantjson::Pointer root;
    if (!std::isfinite(replay.step_rate) || replay.step_rate <= 0.0F) {
        report(root / "step_rate",
               std::format("the step rate must be a positive, finite number of steps a second, "
                           "not {}",
                           replay.step_rate));
    }
    if (replay.scene.empty()) {
        report(root / "scene", "the scene must be the name of a scene, not empty");
    }
    const std::size_t characters = replay.initial.characters.size();
    for (const auto [step_index, step] : std::views::enumerate(replay.steps)) {
        for (const auto [push_index, push] : std::views::enumerate(step.pushes)) {
            if (push.character.index() >= characters) {
                report(root / "steps" / static_cast<std::size_t>(step_index) / "messages" /
                           static_cast<std::size_t>(push_index) / "character",
                       std::format("a push is to one of the replay's {} character{}, so its index "
                                   "is less than {}, not {}",
                                   characters, characters == 1 ? "" : "s", characters,
                                   push.character.index()));
            }
        }
    }
}

} // namespace

std::string write_replay_file(const ReplayDesc& replay) {
    pedantjson::Draft steps = pedantjson::Draft::array();
    for (const RecordedStep& recorded : replay.steps) {
        steps.append(step(recorded));
    }
    pedantjson::Draft root = pedantjson::Draft::object();
    root.append("format", "aplomb.replay")
        .append("version", replay_file_version)
        .append("aplomb_version", replay.aplomb_version)
        .append("jolt_version", replay.jolt_version)
        .append("rig_file_hash", hash_text(replay.rig_file_hash))
        .append("step_rate", replay.step_rate)
        .append("seed", replay.seed)
        .append("scene", replay.scene)
        .append("initial", initial_state(replay.initial))
        .append("steps", std::move(steps));
    std::expected<std::string, pedantjson::WriteError> text = detail::write_json(root);
    APLOMB_ASSERT(text.has_value(), "a replay's numbers are finite and its strings UTF-8");
    return std::move(*text);
}

std::expected<void, FileError> save_replay_file(const std::filesystem::path& path,
                                                const ReplayDesc& replay) {
    return detail::write_text_file(path, kind, write_replay_file(replay));
}

std::expected<ReplayDesc, FileError> load_replay_file(const std::filesystem::path& path) {
    return detail::read_text_file(path, kind).and_then([&](std::string text) {
        return parse_replay_file(std::move(text), path.string());
    });
}

std::expected<ReplayDesc, FileError> parse_replay_file(std::string text, std::string_view path) {
    auto document = detail::parse_json(std::move(text), path);
    if (!document.has_value()) {
        return std::unexpected(detail::invalid_file(kind, path, {std::move(document.error())}));
    }

    JsonFile structure(std::string(path), *document);
    ReplayDesc replay = ReplayFileReader(structure).read();
    if (structure.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(structure).take_problems()));
    }

    JsonFile meaning(std::string(path), *document);
    check_meaning(replay, *document, meaning);
    if (meaning.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(meaning).take_problems()));
    }
    return replay;
}

} // namespace aplomb
