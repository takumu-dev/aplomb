/// \file
/// Metrics files, which record what a scenario's run measured.

#pragma once

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace aplomb {

/// The version of the metrics file format that this writer writes.
inline constexpr std::uint32_t metrics_file_version = 1;

/// The value a run recorded of one metric.
struct MetricValue {
    /// The metric.
    Metric metric{};

    /// Its value.
    double value = 0.0;
};

/// What one run of a scenario measured.
struct RunMetrics {
    /// The scenario's name.
    std::string scenario{};

    /// The step rate it ran at, in steps a second.
    float step_rate = 0.0F;

    /// The value of each metric the scenario records, in the order it lists them.
    std::vector<MetricValue> values{};
};

/// Writes the text of a metrics file.
///
/// A metrics file is a JSON object with the keys `format`, which is `"aplomb.metrics"`, `version`,
/// `scenario`, `step_rate` and `metrics`, an object that maps each metric's name to its value.
/// Each number is written in the shortest form that reads back as the same value, and a value that
/// isn't finite as null.
///
/// \param metrics What the run measured.
/// \return The file's text.
[[nodiscard]] std::string write_metrics_file(const RunMetrics& metrics);

/// Saves a metrics file, replacing any file at its path.
///
/// \param path The file's path.
/// \param metrics What the run measured.
/// \return Nothing, or why the file couldn't be written.
[[nodiscard]] std::expected<void, FileError> save_metrics_file(const std::filesystem::path& path,
                                                               const RunMetrics& metrics);

} // namespace aplomb
