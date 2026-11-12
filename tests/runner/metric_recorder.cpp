#include "metric_recorder.hpp"

#include <aplomb/core/character.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

std::uint64_t nans_in(JPH::Vec3 vector) {
    return static_cast<std::uint64_t>(std::isnan(vector.GetX())) +
           static_cast<std::uint64_t>(std::isnan(vector.GetY())) +
           static_cast<std::uint64_t>(std::isnan(vector.GetZ()));
}

std::uint64_t nans_in(JPH::Quat quaternion) {
    return nans_in(quaternion.GetXYZ()) + static_cast<std::uint64_t>(std::isnan(quaternion.GetW()));
}

// A vector's length, in double precision, so that no speed a float holds overflows on the way.
double length(JPH::Vec3 vector) {
    const auto x = static_cast<double>(vector.GetX());
    const auto y = static_cast<double>(vector.GetY());
    const auto z = static_cast<double>(vector.GetZ());
    return std::sqrt((x * x) + (y * y) + (z * z));
}

} // namespace

std::uint64_t steps_in(float seconds, float step_rate) {
    return static_cast<std::uint64_t>(
        std::llround(static_cast<double>(seconds) * static_cast<double>(step_rate)));
}

std::vector<PartState> part_states(const aplomb::World& world, const aplomb::Character& character) {
    const JPH::BodyInterface& bodies = world.physics_system().GetBodyInterface();
    std::vector<PartState> states;
    states.reserve(character.rig().parts.size());
    for (std::size_t part = 0; part < character.rig().parts.size(); ++part) {
        const JPH::BodyID body =
            character.part_body(aplomb::PartId(static_cast<std::uint32_t>(part)));
        states.push_back({
            .position = bodies.GetPosition(body),
            .rotation = bodies.GetRotation(body),
            .linear_velocity = bodies.GetLinearVelocity(body),
            .angular_velocity = bodies.GetAngularVelocity(body),
        });
    }
    return states;
}

MetricRecorder::MetricRecorder(float step_rate)
    : first_settled_step_(steps_in(aplomb::settling_time, step_rate)) {}

void MetricRecorder::record(std::span<const PartState> parts) {
    ++steps_;
    for (const PartState& part : parts) {
        nan_count_ += nans_in(part.position) + nans_in(part.rotation) +
                      nans_in(part.linear_velocity) + nans_in(part.angular_velocity);
        // A speed that isn't a number is counted as NaNs, and never taken for the largest.
        if (const double speed = length(part.linear_velocity);
            steps_ >= first_settled_step_ && speed > max_speed_) {
            max_speed_ = speed;
        }
    }
}

double MetricRecorder::value(aplomb::Metric metric) const {
    switch (metric) {
    case aplomb::Metric::max_speed_after_3s:
        return steps_ >= first_settled_step_ ? max_speed_
                                             : std::numeric_limits<double>::quiet_NaN();
    case aplomb::Metric::nan_count:
        return static_cast<double>(nan_count_);
    }
    return std::numeric_limits<double>::quiet_NaN();
}
