#pragma once

#include <aplomb/core/character.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

#include <cstdint>
#include <span>
#include <vector>

/// How many steps a span of time takes at a step rate: the whole number nearest it.
///
/// \param seconds The span.
/// \param step_rate The step rate, in steps a second.
std::uint64_t steps_in(float seconds, float step_rate);

/// The state of one part's body after a step.
struct PartState {
    JPH::Vec3 position = JPH::Vec3::sZero();
    JPH::Quat rotation = JPH::Quat::sIdentity();
    JPH::Vec3 linear_velocity = JPH::Vec3::sZero();
    JPH::Vec3 angular_velocity = JPH::Vec3::sZero();
};

/// The state of each of a character's parts, read from its world's physics system, in the order of
/// its rig's parts.
std::vector<PartState> part_states(const aplomb::World& world, const aplomb::Character& character);

/// Records a run's metrics, one step at a time.
class MetricRecorder {
  public:
    /// Starts recording a run at a step rate, in steps a second.
    explicit MetricRecorder(float step_rate);

    /// Records the state of every part after one more step.
    void record(std::span<const PartState> parts);

    /// A metric's value over the steps recorded so far. max_speed_after_3s is NaN until the step
    /// nearest 3 s into the run has been recorded, and state_hash_final, which isn't measured from
    /// the parts, is always NaN.
    [[nodiscard]] double value(aplomb::Metric metric) const;

  private:
    std::uint64_t first_settled_step_;
    std::uint64_t steps_ = 0;
    double max_speed_ = 0.0;
    std::uint64_t nan_count_ = 0;
};
