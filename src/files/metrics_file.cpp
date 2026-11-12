#include <aplomb/files/metrics_file.hpp>

#include "file_reading.hpp"
#include "json_writing.hpp"
#include <aplomb/core/assertion.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <pedantjson/pedantjson.hpp>

#include <cmath>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace aplomb {

namespace {

constexpr std::string_view kind = "metrics file";

// A number, or null if it isn't finite, which JSON can't write.
template <typename T> pedantjson::Draft finite_or_null(T number) {
    return std::isfinite(number) ? pedantjson::Draft(number) : pedantjson::Draft(nullptr);
}

} // namespace

std::string write_metrics_file(const RunMetrics& metrics) {
    pedantjson::Draft values = pedantjson::Draft::object();
    for (const MetricValue& value : metrics.values) {
        values.append(std::string(metric_name(value.metric)), finite_or_null(value.value));
    }
    pedantjson::Draft root = pedantjson::Draft::object();
    root.append("format", "aplomb.metrics")
        .append("version", metrics_file_version)
        .append("scenario", metrics.scenario)
        .append("step_rate", finite_or_null(metrics.step_rate))
        .append("metrics", std::move(values));
    // Every number is finite by now, and every name but the scenario's is aplomb's own, so the
    // scenario's name is all that can stop the file being written.
    std::expected<std::string, pedantjson::WriteError> text = detail::write_json(root);
    APLOMB_ASSERT(text.has_value(), "a metrics file's scenario name is UTF-8");
    return std::move(*text);
}

std::expected<void, FileError> save_metrics_file(const std::filesystem::path& path,
                                                 const RunMetrics& metrics) {
    return detail::write_text_file(path, kind, write_metrics_file(metrics));
}

} // namespace aplomb
