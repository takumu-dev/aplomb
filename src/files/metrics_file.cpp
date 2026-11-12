#include <aplomb/files/metrics_file.hpp>

#include "file_reading.hpp"
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace aplomb {

namespace {

constexpr std::string_view kind = "metrics file";

// A string as JSON writes it, in quotes, with its quotes, backslashes and control characters
// escaped.
std::string json_string(std::string_view text) {
    std::string json = "\"";
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            json += '\\';
            json += character;
        } else if (static_cast<unsigned char>(character) < 0x20) {
            json += std::format("\\u{:04x}", static_cast<unsigned char>(character));
        } else {
            json += character;
        }
    }
    return json + '"';
}

// A number as JSON writes it: in the shortest form that reads back as the same value, or null if
// it isn't finite, which JSON can't write.
template <typename T> std::string json_number(T number) {
    return std::isfinite(number) ? std::format("{}", number) : "null";
}

} // namespace

std::string write_metrics_file(const RunMetrics& metrics) {
    std::string text = std::format("{{\n"
                                   "  \"format\": \"aplomb.metrics\",\n"
                                   "  \"version\": {},\n"
                                   "  \"scenario\": {},\n"
                                   "  \"step_rate\": {},\n"
                                   "  \"metrics\": {{",
                                   metrics_file_version, json_string(metrics.scenario),
                                   json_number(metrics.step_rate));
    for (const MetricValue& value : metrics.values) {
        text += std::format("{}\n    {}: {}", &value == metrics.values.data() ? "" : ",",
                            json_string(metric_name(value.metric)), json_number(value.value));
    }
    text += metrics.values.empty() ? "}\n}\n" : "\n  }\n}\n";
    return text;
}

std::expected<void, FileError> save_metrics_file(const std::filesystem::path& path,
                                                 const RunMetrics& metrics) {
    return detail::write_text_file(path, kind, write_metrics_file(metrics));
}

} // namespace aplomb
