/// \file
/// A scenario, as a scenario file describes it.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace aplomb {

/// How long into a run max_speed_after_3s begins, in seconds.
inline constexpr float settling_time = 3.0F;

/// A quantity that a scenario's run records.
enum class Metric : std::uint8_t {
    /// The largest linear speed of any part, in metres a second, at every step from the one nearest
    /// `settling_time` into the run.
    max_speed_after_3s,

    /// How many NaN values the parts' positions, rotations and linear and angular velocities hold,
    /// summed over every step.
    nan_count,
};

/// The name of a metric, as files give it, such as `nan_count`.
///
/// \param metric The metric.
/// \return Its name.
[[nodiscard]] std::string_view metric_name(Metric metric) noexcept;

/// How a pass criterion compares a metric with its value.
enum class Comparison : std::uint8_t {
    /// The metric is less than the value: `lt`.
    less,

    /// The metric is at most the value: `le`.
    less_or_equal,

    /// The metric is the value: `eq`.
    equal,

    /// The metric is at least the value: `ge`.
    greater_or_equal,

    /// The metric is greater than the value: `gt`.
    greater,
};

/// The name of a comparison, as files give it, such as `lt`.
///
/// \param comparison The comparison.
/// \return Its name.
[[nodiscard]] std::string_view comparison_name(Comparison comparison) noexcept;

/// Whether a metric's value meets a comparison with another value. A NaN meets none.
///
/// \param comparison The comparison.
/// \param measured The metric's value.
/// \param value The value it is compared with.
/// \return Whether it does.
[[nodiscard]] bool compare(Comparison comparison, double measured, double value) noexcept;

/// A value that a metric must reach for its scenario to pass.
struct PassCriterion {
    /// The metric.
    Metric metric{};

    /// How the metric is compared with the value.
    Comparison comparison{};

    /// The value.
    double value = 0.0;
};

/// The pose a scenario's character starts in.
enum class InitialPose : std::uint8_t {
    /// The rig's bind pose.
    bind,
};

/// How a scenario's character starts.
struct InitialState {
    /// Where the rig's space is placed in the world, as `World::add_character` places it.
    JPH::Vec3 root_position = JPH::Vec3::sZero();

    /// The pose the character starts in.
    InitialPose pose = InitialPose::bind;
};

/// A test that opens no window: a character made from a rig file, dropped into a scene, run for a
/// while, with the metrics to record and the values they must reach.
///
/// A scenario has no step rate: whoever runs it chooses one, so that one scenario serves every
/// rate.
struct ScenarioDesc {
    /// The scenario's name.
    std::string name{};

    /// The path of the rig file the character is made from, relative to the scenario file's
    /// directory unless it is absolute.
    std::filesystem::path rig{};

    /// The name of the scene the character is dropped into, such as `flat_ground`.
    std::string scene{};

    /// How long the scenario runs, in seconds.
    float duration = 0.0F;

    /// The seed the world's random generators are derived from.
    std::uint64_t seed = 0;

    /// How the character starts.
    InitialState initial{};

    /// The metrics the run records, each once, in the order the file lists them.
    std::vector<Metric> metrics{};

    /// The values the metrics must reach, in the order of the file. Each is of a metric the run
    /// records.
    std::vector<PassCriterion> pass{};
};

} // namespace aplomb
