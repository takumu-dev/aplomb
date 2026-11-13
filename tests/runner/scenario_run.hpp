#pragma once

#include "host_services.hpp"
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// What kind of failure stopped a scenario from running, or a replay from playing back.
enum class RunErrorCode : std::uint8_t {
    /// The scenario names a scene that doesn't exist.
    unknown_scene,

    /// The world couldn't be created, such as at a step rate it refuses.
    invalid_world,

    /// The scenario's character couldn't be added to the world.
    invalid_character,

    /// The scenario's timeline, or the replay, pushes a part that its rig doesn't have.
    unknown_part,

    /// The replay pushes a character that it doesn't have.
    unknown_character,
};

/// Why a scenario couldn't run, or a replay couldn't play back.
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

    /// The world's state hash before the first step and after each one, so that the hash after
    /// step n is at index n.
    std::vector<std::uint64_t> state_hashes{};

    /// Each step the world took, as it recorded it: the pushes it applied and its state hash after
    /// it.
    std::vector<aplomb::RecordedStep> recorded_steps{};

    /// How many NaN values appeared in the parts' states, whether or not the scenario records
    /// nan_count.
    std::uint64_t nan_count = 0;

    /// How the run met each pass criterion, in the scenario's order.
    std::vector<CriterionResult> criteria{};

    /// Whether the run passed: every criterion is met, and no NaN appeared.
    [[nodiscard]] bool passed() const noexcept;
};

/// What playing a replay back gave.
struct PlaybackResult {
    /// The world's state hash before the first step and after each one, so that the hash after
    /// step n is at index n.
    std::vector<std::uint64_t> state_hashes{};

    /// The first step at which the state hash the replay recorded and the one played back differ,
    /// the recorded one first, or nothing where every step's agree.
    std::optional<aplomb::StateHashDifference> first_differing_step{};
};

/// The first step at which two runs' state hashes differ, if any does.
///
/// \param first The first run's state hash at each step.
/// \param second The second run's, of as many steps.
/// \return The step, with both hashes at it, or nothing where every step's hashes agree.
[[nodiscard]] std::optional<aplomb::StateHashDifference>
first_differing_step(std::span<const std::uint64_t> first, std::span<const std::uint64_t> second);

/// Runs a scenario at a step rate: drops a character made from the rig into the scenario's scene,
/// steps the world for the whole number of steps nearest the scenario's duration, sending the
/// pushes of its timeline as it goes, and records its metrics and the world's state hash after
/// every step.
///
/// The pushes are sent in the order of their times, and those of one time in the timeline's order.
/// A push nearest the end of the run is sent before the last step, since nothing sent after it
/// would take effect.
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

/// The replay of a run of a scenario, which `play_replay` plays back to the run's state hashes.
///
/// \param scenario The scenario.
/// \param rig_file_hash The hash of the rig file the scenario's rig was read from.
/// \param result What the run gave.
/// \return The replay, with this build's versions of aplomb and Jolt.
[[nodiscard]] aplomb::ReplayDesc replay_of(const aplomb::ScenarioDesc& scenario,
                                           std::uint64_t rig_file_hash, const RunResult& result);

/// Plays a replay back: builds its scene into a world at its step rate, adds its characters made
/// from the rig, then sends each step's pushes and steps the world, comparing the state hash before
/// the first step and after each one with the replay's.
///
/// \param replay The replay, whose rig file the rig was read from.
/// \param rig The rig.
/// \param services The job system and temporary allocator the world is lent.
/// \return What playing the replay back gave, or why it couldn't be played back.
[[nodiscard]] std::expected<PlaybackResult, RunError>
play_replay(const aplomb::ReplayDesc& replay, const aplomb::RigDesc& rig, HostServices& services);
