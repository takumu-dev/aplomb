// The scenario runner: runs a scenario file at a step rate, opening no window, writes what the run
// measured to a metrics file, and reports whether the run met the scenario's pass criteria. It
// plays replays back too.
//
//   aplomb_runner run <scenario file> --step-rate <steps a second> --metrics <metrics file>
//       [--golden-hash <state hash>] [--replay <replay file>]
//   aplomb_runner determinism <scenario file> ...
//   aplomb_runner replay <replay file> --rig <rig file> --metrics <metrics file>
//
// `run` runs the scenario once. `determinism` runs it twice and compares the two runs' state hashes
// at every step; its metrics file is the first run's, with the first step at which they differed.
// A golden hash, as 16 hexadecimal digits, is what the final state hash must be. A replay file, if
// given, is where the replay of the (first) run is saved.
//
// `replay` plays a replay back with the rig file it was recorded with, comparing the state hash
// before the first step and after each one with the replay's. Its metrics file names the replay by
// its file's stem, and gives the first step at which they differed.
//
// It exits with 0 when the run passes, 1 when it misses a pass criterion, a NaN appears, the final
// state hash isn't the golden hash, the two runs differ or the replay differs, and 2 when the
// scenario or the replay can't run at all.

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "scenario_run.hpp"
#include <aplomb/core/version.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/replay_file.hpp>
#include <aplomb/files/rig_file.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/files/scenario_file.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace {

constexpr int passed = 0;
constexpr int failed = 1;
constexpr int unable_to_run = 2;

constexpr std::string_view usage =
    "usage: aplomb_runner run <scenario file> --step-rate <steps a second> --metrics <metrics "
    "file> [--golden-hash <state hash>] [--replay <replay file>]\n"
    "       aplomb_runner determinism <scenario file> --step-rate <steps a second> --metrics "
    "<metrics file> [--golden-hash <state hash>] [--replay <replay file>]\n"
    "       aplomb_runner replay <replay file> --rig <rig file> --metrics <metrics file>";

// How the runner runs a scenario.
enum class Mode : std::uint8_t {
    // Once.
    run,

    // Twice, comparing the two runs' state hashes at every step.
    determinism,
};

// A scenario to run, as the command line asks for it, recording its replay to `replay` if that is
// given.
struct ScenarioInvocation {
    Mode mode = Mode::run;
    std::filesystem::path scenario;
    std::string_view step_rate;
    std::filesystem::path metrics;
    std::optional<std::string_view> golden_hash;
    std::optional<std::filesystem::path> replay;
};

// A replay to play back, as the command line asks for it.
struct ReplayInvocation {
    std::filesystem::path replay;
    std::filesystem::path rig;
    std::filesystem::path metrics;
};

// What the command line asks for.
using Invocation = std::variant<ScenarioInvocation, ReplayInvocation>;

std::optional<Mode> parse_mode(std::string_view text) {
    if (text == "run") {
        return Mode::run;
    }
    if (text == "determinism") {
        return Mode::determinism;
    }
    return std::nullopt;
}

std::optional<Invocation> parse_invocation(std::span<char* const> arguments) {
    if (arguments.size() < 2) {
        return std::nullopt;
    }
    const bool replays = std::string_view(arguments[1]) == "replay";
    const std::optional<Mode> mode = parse_mode(arguments[1]);
    if (!replays && !mode.has_value()) {
        return std::nullopt;
    }
    std::optional<std::filesystem::path> file;
    std::optional<std::string_view> step_rate;
    std::optional<std::filesystem::path> metrics;
    std::optional<std::string_view> golden_hash;
    std::optional<std::filesystem::path> replay;
    std::optional<std::filesystem::path> rig;
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        const bool has_value = index + 1 < arguments.size();
        if (argument == "--metrics" && has_value && !metrics.has_value()) {
            metrics = arguments[++index];
        } else if (argument == "--step-rate" && has_value && !replays && !step_rate.has_value()) {
            step_rate = arguments[++index];
        } else if (argument == "--golden-hash" && has_value && !replays &&
                   !golden_hash.has_value()) {
            golden_hash = arguments[++index];
        } else if (argument == "--replay" && has_value && !replays && !replay.has_value()) {
            replay = arguments[++index];
        } else if (argument == "--rig" && has_value && replays && !rig.has_value()) {
            rig = arguments[++index];
        } else if (!argument.starts_with("--") && !file.has_value()) {
            file = argument;
        } else {
            return std::nullopt;
        }
    }
    if (!file.has_value() || !metrics.has_value()) {
        return std::nullopt;
    }
    if (replays) {
        if (!rig.has_value()) {
            return std::nullopt;
        }
        return ReplayInvocation{.replay = *file, .rig = *rig, .metrics = *metrics};
    }
    if (!mode.has_value() || !step_rate.has_value()) {
        return std::nullopt;
    }
    return ScenarioInvocation{
        .mode = *mode,
        .scenario = *file,
        .step_rate = *step_rate,
        .metrics = *metrics,
        .golden_hash = golden_hash,
        .replay = replay,
    };
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

// A state hash is exactly 16 hexadecimal digits, as metrics files and golden hashes give it.
std::optional<std::uint64_t> parse_state_hash(std::string_view text) {
    constexpr std::size_t digits = 16;
    constexpr int base = 16;
    std::uint64_t hash = 0;
    const char* const text_end = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), text_end, hash, base);
    if (text.size() != digits || error != std::errc() || end != text_end) {
        return std::nullopt;
    }
    return hash;
}

std::string format_value(const std::variant<double, std::uint64_t>& value) {
    if (const auto* const hash = std::get_if<std::uint64_t>(&value)) {
        return std::format("{:016x}", *hash);
    }
    return std::format("{}", std::get<double>(value));
}

void report(const RunResult& result) {
    std::println("{} at {} Hz: {} steps", result.metrics.scenario, result.metrics.step_rate,
                 result.steps);
    for (const aplomb::MetricValue& value : result.metrics.values) {
        std::println("{} = {}", aplomb::metric_name(value.metric), format_value(value.value));
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
}

// Runs a scenario file once or twice, as the invocation asks.
int run_scenario_file(const ScenarioInvocation& invocation) {
    const std::optional<float> step_rate = parse_step_rate(invocation.step_rate);
    if (!step_rate.has_value()) {
        std::println(stderr,
                     "error: the step rate must be a positive, finite number of steps a second, "
                     "not \"{}\"",
                     invocation.step_rate);
        return unable_to_run;
    }

    std::optional<std::uint64_t> golden_hash;
    if (invocation.golden_hash.has_value()) {
        golden_hash = parse_state_hash(*invocation.golden_hash);
        if (!golden_hash.has_value()) {
            std::println(stderr, "error: the golden hash must be 16 hexadecimal digits, not \"{}\"",
                         *invocation.golden_hash);
            return unable_to_run;
        }
    }

    const auto scenario = aplomb::load_scenario_file(invocation.scenario);
    if (!scenario.has_value()) {
        std::println(stderr, "error: {}", scenario.error());
        return unable_to_run;
    }
    const std::filesystem::path rig_path = invocation.scenario.parent_path() / scenario->rig;
    const auto rig = aplomb::load_rig_file(rig_path);
    if (!rig.has_value()) {
        std::println(stderr, "error: {}", rig.error());
        return unable_to_run;
    }

    const JoltRuntime runtime;
    HostServices services;
    auto result = run_scenario(*scenario, *rig, *step_rate, services);
    if (!result.has_value()) {
        std::println(stderr, "error: {}", result.error());
        return unable_to_run;
    }
    const std::uint64_t final_hash = result->state_hashes.back();
    const bool golden_hash_met = !golden_hash.has_value() || final_hash == *golden_hash;
    if (invocation.mode == Mode::determinism) {
        const auto second = run_scenario(*scenario, *rig, *step_rate, services);
        if (!second.has_value()) {
            std::println(stderr, "error: {}", second.error());
            return unable_to_run;
        }
        result->metrics.first_differing_step =
            first_differing_step(result->state_hashes, second->state_hashes);
    }
    if (const auto saved = aplomb::save_metrics_file(invocation.metrics, result->metrics);
        !saved.has_value()) {
        std::println(stderr, "error: {}", saved.error());
        return unable_to_run;
    }
    if (invocation.replay.has_value()) {
        // The rig file was read a moment ago, so it fails to read again only if it has gone since.
        const auto rig_file_hash = aplomb::load_rig_file_hash(rig_path);
        if (!rig_file_hash.has_value()) {
            std::println(stderr, "error: {}", rig_file_hash.error());
            return unable_to_run;
        }
        const aplomb::ReplayDesc replay = replay_of(*scenario, *rig_file_hash, *result);
        if (const auto saved = aplomb::save_replay_file(*invocation.replay, replay);
            !saved.has_value()) {
            std::println(stderr, "error: {}", saved.error());
            return unable_to_run;
        }
    }

    report(*result);
    if (!golden_hash_met) {
        std::println("state_hash_final {:016x} differs from the golden hash {:016x}", final_hash,
                     *golden_hash);
    }
    if (invocation.mode == Mode::determinism) {
        if (const auto& difference = result->metrics.first_differing_step) {
            std::println("the runs differed first at step {}, with the state hashes {:016x} and "
                         "{:016x}",
                         difference->step, difference->state_hashes[0],
                         difference->state_hashes[1]);
        } else {
            std::println("both runs gave the same state hash before the first step and after each "
                         "of {}",
                         result->steps);
        }
    }
    const bool run_passed =
        result->passed() && golden_hash_met && !result->metrics.first_differing_step.has_value();
    std::println("{}", run_passed ? "passed" : "failed");
    return run_passed ? passed : failed;
}

// Plays a replay file back with the rig file the invocation names.
int play_replay_file(const ReplayInvocation& invocation) {
    const auto replay = aplomb::load_replay_file(invocation.replay);
    if (!replay.has_value()) {
        std::println(stderr, "error: {}", replay.error());
        return unable_to_run;
    }
    // A replay played back with another rig than its own would differ for no reason worth finding.
    const auto rig_file_hash = aplomb::load_rig_file_hash(invocation.rig);
    if (!rig_file_hash.has_value()) {
        std::println(stderr, "error: {}", rig_file_hash.error());
        return unable_to_run;
    }
    if (*rig_file_hash != replay->rig_file_hash) {
        std::println(stderr,
                     "error: the replay was recorded with a rig file whose hash is {:016x}, but "
                     "the hash of \"{}\" is {:016x}",
                     replay->rig_file_hash, invocation.rig.string(), *rig_file_hash);
        return unable_to_run;
    }
    const auto rig = aplomb::load_rig_file(invocation.rig);
    if (!rig.has_value()) {
        std::println(stderr, "error: {}", rig.error());
        return unable_to_run;
    }

    const JoltRuntime runtime;
    HostServices services;
    const auto result = play_replay(*replay, *rig, services);
    if (!result.has_value()) {
        std::println(stderr, "error: {}", result.error());
        return unable_to_run;
    }
    const aplomb::RunMetrics metrics{
        .scenario = invocation.replay.stem().string(),
        .step_rate = replay->step_rate,
        .values = {},
        .first_differing_step = result->first_differing_step,
    };
    if (const auto saved = aplomb::save_metrics_file(invocation.metrics, metrics);
        !saved.has_value()) {
        std::println(stderr, "error: {}", saved.error());
        return unable_to_run;
    }

    std::println("{} at {} Hz: {} steps", metrics.scenario, metrics.step_rate,
                 replay->steps.size());
    // Another version may step differently, which would explain a replay that differs.
    if (replay->aplomb_version != aplomb::version() ||
        replay->jolt_version != aplomb::jolt_version()) {
        std::println("the replay was recorded by aplomb {} with Jolt {}, and is played back by "
                     "aplomb {} with Jolt {}",
                     replay->aplomb_version, replay->jolt_version, aplomb::version(),
                     aplomb::jolt_version());
    }
    if (const auto& difference = result->first_differing_step) {
        std::println("the replay differed first at step {}, with the recorded state hash {:016x} "
                     "and the played back {:016x}",
                     difference->step, difference->state_hashes[0], difference->state_hashes[1]);
    } else {
        std::println("every state hash was the replay's, before the first step and after each of "
                     "{}",
                     replay->steps.size());
    }
    const bool played_back = !result->first_differing_step.has_value();
    std::println("{}", played_back ? "passed" : "failed");
    return played_back ? passed : failed;
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
    if (const auto* const replay = std::get_if<ReplayInvocation>(&*invocation)) {
        return play_replay_file(*replay);
    }
    return run_scenario_file(*std::get_if<ScenarioInvocation>(&*invocation));
}
