#include <aplomb/files/scenario_file.hpp>

#include "file_reading.hpp"
#include "json_file.hpp"
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aplomb {

namespace {

using detail::Field;
using detail::JsonFile;

constexpr std::string_view kind = "scenario file";

constexpr std::array all_metrics{
    Metric::max_speed_after_3s,
    Metric::nan_count,
    Metric::state_hash_final,
};

constexpr std::array all_comparisons{
    Comparison::less,    Comparison::less_or_equal, Comparison::equal, Comparison::greater_or_equal,
    Comparison::greater,
};

// Names in quotes, as messages list the choices a value has: `"a", "b" or "c"`.
template <typename T, std::size_t Count>
std::string choices(const std::array<T, Count>& values, std::string_view (*name)(T) noexcept) {
    std::string text;
    for (std::size_t index = 0; const T value : values) {
        if (index > 0) {
            text += index + 1 == Count ? " or " : ", ";
        }
        text += std::format("\"{}\"", name(value));
        ++index;
    }
    return text;
}

// Reads the structure of a scenario file into a scenario. Each part of the reader fills in what it
// can and reports what it can't, so that one pass finds every problem; the scenario is used only
// if none was reported.
class ScenarioFileReader {
  public:
    explicit ScenarioFileReader(JsonFile& file) : file_(file) {}

    ScenarioDesc read() {
        ScenarioDesc scenario;
        std::optional<detail::ObjectReader> root =
            detail::read_object(file_.get().root(), std::string(kind), file_);
        if (!root.has_value()) {
            return scenario;
        }
        // A file of another format or of a newer version can't be read any further, since its
        // other keys can't be known.
        if (!detail::read_format_and_version(*root, kind, "aplomb.scenario", scenario_file_version,
                                             file_)
                 .has_value()) {
            return scenario;
        }
        detail::read_into(root->take("name"), scenario.name, detail::read_string, file_);
        detail::read_into(root->take("rig"), scenario.rig, detail::read_string, file_);
        detail::read_into(root->take("scene"), scenario.scene, detail::read_string, file_);
        detail::read_into(root->take("duration_s"), scenario.duration, detail::read_number, file_);
        detail::read_into(root->take("seed"), scenario.seed, detail::read_seed, file_);
        if (const std::optional<Field> initial = root->take("initial")) {
            scenario.initial = detail::read_initial_state(*initial, "initial state", file_);
        }
        if (const std::optional<Field> timeline = root->take("timeline")) {
            scenario.timeline = read_timeline(*timeline);
        }
        if (const std::optional<Field> metrics = root->take("metrics")) {
            scenario.metrics = read_metrics(*metrics);
        }
        if (const std::optional<Field> pass = root->take("pass")) {
            scenario.pass = read_pass(*pass);
        }
        root->finish();
        return scenario;
    }

  private:
    // Version 1 knows one message, a push. An entry whose message isn't known can't have its other
    // keys checked, so its reader doesn't finish.
    std::vector<TimelinePush> read_timeline(const Field& field) {
        std::vector<TimelinePush> timeline;
        for (const Field& entry : detail::read_array(field, file_).value_or({})) {
            std::optional<detail::ObjectReader> reader =
                detail::read_object(entry, "timeline entry", file_);
            if (!reader.has_value()) {
                continue;
            }
            TimelinePush push;
            detail::read_into(reader->take("t"), push.time, detail::read_number, file_);
            const std::optional<Field> message = reader->take("message");
            if (!message.has_value()) {
                continue;
            }
            const std::optional<std::string> name = detail::read_string(*message, file_);
            if (name != "push") {
                if (name.has_value()) {
                    file_.get().report_at(
                        *message, FileProblemCode::invalid_value,
                        std::format(R"(the message of a timeline entry is "push", not "{}")",
                                    *name));
                }
                continue;
            }
            detail::read_into(reader->take("part"), push.part, detail::read_string, file_);
            detail::read_into(reader->take("point"), push.point, detail::read_vector, file_);
            detail::read_into(reader->take("impulse"), push.impulse, detail::read_vector, file_);
            reader->finish();
            timeline.push_back(std::move(push));
        }
        return timeline;
    }

    std::vector<Metric> read_metrics(const Field& field) {
        std::vector<Metric> metrics;
        std::vector<std::size_t> lines;
        for (const Field& element : detail::read_array(field, file_).value_or({})) {
            const std::optional<std::string> name = detail::read_string(element, file_);
            if (!name.has_value()) {
                continue;
            }
            const auto* const metric = std::ranges::find(all_metrics, *name, metric_name);
            if (metric == all_metrics.end()) {
                file_.get().report_at(element, FileProblemCode::invalid_value,
                                      std::format("a metric is {}, not \"{}\"",
                                                  choices(all_metrics, metric_name), *name));
                continue;
            }
            const std::size_t line = file_.get().document().position(element.value.offset()).line;
            if (const auto listed = std::ranges::find(metrics, *metric); listed != metrics.end()) {
                const auto index = static_cast<std::size_t>(listed - metrics.begin());
                file_.get().report_at(
                    element, FileProblemCode::invalid_value,
                    std::format(R"(the metric "{}" is listed already, on line {})", *name,
                                lines[index]));
                continue;
            }
            metrics.push_back(*metric);
            lines.push_back(line);
        }
        return metrics;
    }

    // A criterion, and where in the text its value is, so that the criteria can be put in the
    // order of the file.
    struct PlacedCriterion {
        std::size_t offset = 0;
        PassCriterion criterion;
    };

    std::vector<PassCriterion> read_pass(const Field& field) {
        std::optional<detail::ObjectReader> reader =
            detail::read_object(field, "set of pass criteria", file_);
        if (!reader.has_value()) {
            return {};
        }
        std::vector<PlacedCriterion> placed;
        for (const Metric metric : all_metrics) {
            const std::optional<Field> criterion = reader->take_optional(metric_name(metric));
            if (!criterion.has_value()) {
                continue;
            }
            if (metric == Metric::state_hash_final) {
                file_.get().report_at(
                    *criterion, FileProblemCode::invalid_value,
                    std::format(R"(the metric "{}" has no pass criterion, since a golden hash )"
                                "checks it",
                                metric_name(metric)));
                continue;
            }
            read_criterion(*criterion, metric, placed);
        }
        reader->finish();
        std::ranges::sort(placed, {}, &PlacedCriterion::offset);
        std::vector<PassCriterion> pass;
        pass.reserve(placed.size());
        for (const PlacedCriterion& criterion : placed) {
            pass.push_back(criterion.criterion);
        }
        return pass;
    }

    void read_criterion(const Field& field, Metric metric, std::vector<PlacedCriterion>& placed) {
        std::optional<detail::ObjectReader> reader =
            detail::read_object(field, "pass criterion", file_);
        if (!reader.has_value()) {
            return;
        }
        bool compares = false;
        for (const Comparison comparison : all_comparisons) {
            const std::optional<Field> value = reader->take_optional(comparison_name(comparison));
            if (!value.has_value()) {
                continue;
            }
            compares = true;
            if (const std::optional<double> number = detail::read_double(*value, file_)) {
                placed.push_back({
                    .offset = value->value.offset(),
                    .criterion = {.metric = metric, .comparison = comparison, .value = *number},
                });
            }
        }
        if (!compares) {
            file_.get().report_at(field, FileProblemCode::invalid_value,
                                  std::format("a pass criterion compares its metric with {}",
                                              choices(all_comparisons, comparison_name)));
        }
        reader->finish();
    }

    std::reference_wrapper<JsonFile> file_;
};

// Reports what is wrong with what a scenario file describes, once its structure has been read.
void check_meaning(const ScenarioDesc& scenario, const pedantjson::Document& document,
                   JsonFile& file) {
    const auto report = [&](const pedantjson::Pointer& pointer, std::string message) {
        file.report(FileProblemCode::invalid_value, pointer, document.nearest(pointer).offset(),
                    std::move(message));
    };
    const pedantjson::Pointer root;
    if (scenario.rig.empty()) {
        report(root / "rig", "the rig must be the path of a rig file, not empty");
    }
    if (scenario.scene.empty()) {
        report(root / "scene", "the scene must be the name of a scene, not empty");
    }
    // A duration that is wrong itself is all that is reported of it.
    const bool duration_valid = std::isfinite(scenario.duration) && scenario.duration > 0.0F;
    if (!duration_valid) {
        report(root / "duration_s",
               std::format("the duration must be a positive, finite number of seconds, not {}",
                           scenario.duration));
    }
    for (const auto [index, push] : std::views::enumerate(scenario.timeline)) {
        const pedantjson::Pointer entry = root / "timeline" / static_cast<std::size_t>(index);
        if (push.part.empty()) {
            report(entry / "part", "the part pushed must be the name of a part, not empty");
        }
        if (push.time < 0.0F) {
            report(entry / "t",
                   std::format("a push's time must be at least 0 s, not {} s", push.time));
        } else if (duration_valid && push.time >= scenario.duration) {
            // A push sent at the end of the run would never take effect.
            report(entry / "t",
                   std::format("a push's time must be less than the duration of {} s, not {} s",
                               scenario.duration, push.time));
        }
    }
    for (const auto [index, metric] : std::views::enumerate(scenario.metrics)) {
        if (metric == Metric::max_speed_after_3s && duration_valid &&
            scenario.duration < settling_time) {
            report(root / "metrics" / static_cast<std::size_t>(index),
                   std::format(R"(the metric "{}" needs a duration of at least {} s, not {} s)",
                               metric_name(metric), settling_time, scenario.duration));
        }
    }
    std::vector<Metric> reported;
    for (const PassCriterion& criterion : scenario.pass) {
        if (std::ranges::contains(scenario.metrics, criterion.metric) ||
            std::ranges::contains(reported, criterion.metric)) {
            continue;
        }
        reported.push_back(criterion.metric);
        report(root / "pass" / metric_name(criterion.metric),
               std::format(R"(the metric "{}" has a pass criterion, but isn't in "metrics")",
                           metric_name(criterion.metric)));
    }
}

} // namespace

std::expected<ScenarioDesc, FileError> load_scenario_file(const std::filesystem::path& path) {
    return detail::read_text_file(path, kind).and_then([&](std::string text) {
        return parse_scenario_file(std::move(text), path.string());
    });
}

std::expected<ScenarioDesc, FileError> parse_scenario_file(std::string text,
                                                           std::string_view path) {
    auto document = detail::parse_json(std::move(text), path);
    if (!document.has_value()) {
        return std::unexpected(detail::invalid_file(kind, path, {std::move(document.error())}));
    }

    JsonFile structure(std::string(path), *document);
    ScenarioDesc scenario = ScenarioFileReader(structure).read();
    if (structure.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(structure).take_problems()));
    }

    JsonFile meaning(std::string(path), *document);
    check_meaning(scenario, *document, meaning);
    if (meaning.has_problems()) {
        return std::unexpected(
            detail::invalid_file(kind, path, std::move(meaning).take_problems()));
    }
    return scenario;
}

} // namespace aplomb
