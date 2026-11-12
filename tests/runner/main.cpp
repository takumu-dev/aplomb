// The scenario runner: runs a scenario file at a step rate, opening no window, writes what the run
// measured to a metrics file, and reports whether the run met the scenario's pass criteria.
//
//   aplomb_runner run <scenario file> --step-rate <steps a second> --metrics <metrics file>
//
// It exits with 0 when the run passes, 1 when it misses a pass criterion or a NaN appears, and 2
// when the scenario can't run at all.

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "scenario_run.hpp"
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/rig_file.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/files/scenario_file.hpp>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <system_error>

namespace {

constexpr int passed = 0;
constexpr int failed = 1;
constexpr int unable_to_run = 2;

constexpr std::string_view usage = "usage: aplomb_runner run <scenario file> --step-rate <steps a "
                                   "second> --metrics <metrics file>";

// What the command line asks for.
struct Invocation {
    std::filesystem::path scenario;
    std::string_view step_rate;
    std::filesystem::path metrics;
};

std::optional<Invocation> parse_invocation(std::span<char* const> arguments) {
    if (arguments.size() < 2 || std::string_view(arguments[1]) != "run") {
        return std::nullopt;
    }
    std::optional<std::filesystem::path> scenario;
    std::optional<std::string_view> step_rate;
    std::optional<std::filesystem::path> metrics;
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        const bool has_value = index + 1 < arguments.size();
        if (argument == "--step-rate" && has_value && !step_rate.has_value()) {
            step_rate = arguments[++index];
        } else if (argument == "--metrics" && has_value && !metrics.has_value()) {
            metrics = arguments[++index];
        } else if (!argument.starts_with("--") && !scenario.has_value()) {
            scenario = argument;
        } else {
            return std::nullopt;
        }
    }
    if (!scenario.has_value() || !step_rate.has_value() || !metrics.has_value()) {
        return std::nullopt;
    }
    return Invocation{.scenario = *scenario, .step_rate = *step_rate, .metrics = *metrics};
}

std::optional<float> parse_step_rate(std::string_view text) {
    float step_rate = 0.0F;
    const char* const text_end = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), text_end, step_rate);
    if (error != std::errc() || end != text_end || !std::isfinite(step_rate) || step_rate <= 0.0F) {
        return std::nullopt;
    }
    return step_rate;
}

void report(const RunResult& result) {
    std::println("{} at {} Hz: {} steps", result.metrics.scenario, result.metrics.step_rate,
                 result.steps);
    for (const aplomb::MetricValue& value : result.metrics.values) {
        std::println("{} = {}", aplomb::metric_name(value.metric), value.value);
    }
    for (const CriterionResult& criterion : result.criteria) {
        std::println("{} {} {}: {}, at {}", aplomb::metric_name(criterion.criterion.metric),
                     aplomb::comparison_name(criterion.criterion.comparison),
                     criterion.criterion.value, criterion.passed ? "met" : "missed",
                     criterion.value);
    }
    if (result.nan_count > 0) {
        std::println("{} NaN values appeared", result.nan_count);
    }
    std::println("{}", result.passed() ? "passed" : "failed");
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char* argv[]) {
    const std::optional<Invocation> invocation =
        parse_invocation(std::span(argv, static_cast<std::size_t>(argc)));
    if (!invocation.has_value()) {
        std::println(stderr, "{}", usage);
        return unable_to_run;
    }
    const std::optional<float> step_rate = parse_step_rate(invocation->step_rate);
    if (!step_rate.has_value()) {
        std::println(stderr,
                     "error: the step rate must be a positive, finite number of steps a second, "
                     "not \"{}\"",
                     invocation->step_rate);
        return unable_to_run;
    }

    const auto scenario = aplomb::load_scenario_file(invocation->scenario);
    if (!scenario.has_value()) {
        std::println(stderr, "error: {}", scenario.error());
        return unable_to_run;
    }
    const auto rig = aplomb::load_rig_file(invocation->scenario.parent_path() / scenario->rig);
    if (!rig.has_value()) {
        std::println(stderr, "error: {}", rig.error());
        return unable_to_run;
    }

    const JoltRuntime runtime;
    HostServices services;
    const auto result = run_scenario(*scenario, *rig, *step_rate, services);
    if (!result.has_value()) {
        std::println(stderr, "error: {}", result.error());
        return unable_to_run;
    }
    if (const auto saved = aplomb::save_metrics_file(invocation->metrics, result->metrics);
        !saved.has_value()) {
        std::println(stderr, "error: {}", saved.error());
        return unable_to_run;
    }
    report(*result);
    return result->passed() ? passed : failed;
}
