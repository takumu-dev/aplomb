#pragma once

#include "host_services.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

/// What kind of failure stopped a scenario from running.
enum class RunErrorCode : std::uint8_t {
    /// The scenario names a scene that doesn't exist.
    unknown_scene,

    /// The world couldn't be created, such as at a step rate it refuses.
    invalid_world,

    /// The scenario's character couldn't be added to the world.
    invalid_character,
};

/// Why a scenario couldn't run.
struct RunError {
    /// What kind of failure it was.
    RunErrorCode code{};

    /// What went wrong, for a person to read.
    std::string message{};
};

/// Formats a run error as its message.
template <> struct std::formatter<RunError> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const RunError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};

/// How a run met one of its scenario's pass criteria.
struct CriterionResult {
    /// The criterion.
    aplomb::PassCriterion criterion{};

    /// The value its metric reached.
    double value = 0.0;

    /// Whether the value meets the criterion.
    bool passed = false;
};

/// What a run of a scenario measured, and whether it passed.
struct RunResult {
    /// The value of each metric the scenario records.
    aplomb::RunMetrics metrics{};

    /// How many steps the run took.
    std::uint64_t steps = 0;

    /// How many NaN values appeared in the parts' states, whether or not the scenario records
    /// nan_count.
    std::uint64_t nan_count = 0;

    /// How the run met each pass criterion, in the scenario's order.
    std::vector<CriterionResult> criteria{};

    /// Whether the run passed: every criterion is met, and no NaN appeared.
    [[nodiscard]] bool passed() const noexcept;
};

/// Runs a scenario at a step rate: drops a character made from the rig into the scenario's scene,
/// steps the world for the whole number of steps nearest the scenario's duration, and records its
/// metrics after every step.
///
/// \param scenario The scenario.
/// \param rig The rig its rig file describes.
/// \param step_rate The step rate, in steps a second.
/// \param services The job system and temporary allocator the world is lent.
/// \return What the run measured, or why it couldn't run.
[[nodiscard]] std::expected<RunResult, RunError> run_scenario(const aplomb::ScenarioDesc& scenario,
                                                              const aplomb::RigDesc& rig,
                                                              float step_rate,
                                                              HostServices& services);
