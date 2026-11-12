/// \file
/// Metrics files, which record what a scenario's run measured.

#pragma once

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace aplomb {

/// The version of the metrics file format that this writer writes.
inline constexpr std::uint32_t metrics_file_version = 1;

/// The value a run recorded of one metric.
struct MetricValue {
    /// The metric.
    Metric metric{};

    /// Its value: a state hash for `state_hash_final`, and a number for every other metric.
    std::variant<double, std::uint64_t> value{};
};

/// The first step at which two runs of a scenario had different state hashes.
struct StateHashDifference {
    /// How many steps each run had taken, 0 being before the first.
    std::uint64_t step = 0;

    /// Each run's state hash at that step, the first run's first.
    std::array<std::uint64_t, 2> state_hashes{};
};

/// What one run of a scenario measured.
struct RunMetrics {
    /// The scenario's name.
    std::string scenario{};

    /// The step rate it ran at, in steps a second.
    float step_rate = 0.0F;

    /// The value of each metric the scenario records, in the order it lists them.
    std::vector<MetricValue> values{};

    /// Where the run was one of two that should have run alike, the first step at which they
    /// didn't.
    std::optional<StateHashDifference> first_differing_step{};
};

/// Writes the text of a metrics file.
///
/// A metrics file is a JSON object with the keys `format`, which is `"aplomb.metrics"`, `version`,
/// `scenario`, `step_rate` and `metrics`, an object that maps each metric's name to its value,
/// and `first_differing_step` where two runs differed: an object of the `step` and the two runs'
/// `state_hashes`. Each number is written in the shortest form that reads back as the same value,
/// and a value that isn't finite as null. A state hash is a string of 16 lower-case hexadecimal
/// digits, since a 64-bit integer is more than many readers of JSON hold exactly.
///
/// \param metrics What the run measured, whose scenario name must be UTF-8.
/// \return The file's text.
[[nodiscard]] std::string write_metrics_file(const RunMetrics& metrics);

/// Saves a metrics file, replacing any file at its path.
///
/// \param path The file's path.
/// \param metrics What the run measured, whose scenario name must be UTF-8.
/// \return Nothing, or why the file couldn't be written.
[[nodiscard]] std::expected<void, FileError> save_metrics_file(const std::filesystem::path& path,
                                                               const RunMetrics& metrics);

} // namespace aplomb
