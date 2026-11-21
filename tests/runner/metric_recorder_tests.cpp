#include "metric_recorder.hpp"

#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

namespace {

constexpr float not_a_number = std::numeric_limits<float>::quiet_NaN();

// Parts at rest but for each one's linear velocity.
std::vector<PartState> moving_at(std::initializer_list<JPH::Vec3> velocities) {
    std::vector<PartState> parts;
    for (const JPH::Vec3 velocity : velocities) {
        parts.push_back({.linear_velocity = velocity});
    }
    return parts;
}

} // namespace

TEST_CASE("max_speed_after_3s is the largest speed of any part at the steps from 3 s on",
          "[runner]") {
    // At 2 Hz, the sixth step ends 3 s into the run.
    MetricRecorder recorder(2.0F);
    for (int step = 0; step < 5; ++step) {
        recorder.record(moving_at({JPH::Vec3(100.0F, 0.0F, 0.0F)}));
    }

    recorder.record(moving_at({JPH::Vec3(1.0F, 0.0F, 0.0F), JPH::Vec3(0.0F, 3.0F, -4.0F)}));
    recorder.record(moving_at({JPH::Vec3(2.0F, 0.0F, 0.0F), JPH::Vec3::sZero()}));

    CHECK(recorder.value(aplomb::Metric::max_speed_after_3s) == 5.0);
}

TEST_CASE("max_speed_after_3s isn't a number before a step 3 s into the run", "[runner]") {
    MetricRecorder recorder(60.0F);
    for (int step = 0; step < 179; ++step) {
        recorder.record(moving_at({JPH::Vec3::sZero()}));
    }

    CHECK(std::isnan(recorder.value(aplomb::Metric::max_speed_after_3s)));
    recorder.record(moving_at({JPH::Vec3::sZero()}));
    CHECK(recorder.value(aplomb::Metric::max_speed_after_3s) == 0.0);
}

TEST_CASE("max_speed_after_3s begins at the step nearest 3 s, which a run of 3 s takes at any step "
          "rate",
          "[runner]") {
    // At 60.1 Hz, a run of 3 s takes 180 steps, the 180th ending a moment before 3 s.
    MetricRecorder recorder(60.1F);
    for (int step = 0; step < 179; ++step) {
        recorder.record(moving_at({JPH::Vec3::sZero()}));
    }

    CHECK(std::isnan(recorder.value(aplomb::Metric::max_speed_after_3s)));
    recorder.record(moving_at({JPH::Vec3(0.5F, 0.0F, 0.0F)}));
    CHECK(recorder.value(aplomb::Metric::max_speed_after_3s) == 0.5);
}

TEST_CASE("nan_count counts every NaN in the parts' positions, rotations and velocities, at every "
          "step",
          "[runner]") {
    MetricRecorder recorder(60.0F);
    const PartState still;
    const PartState lost{
        .position = JPH::Vec3(not_a_number, 0.0F, not_a_number),
        .rotation = JPH::Quat(0.0F, not_a_number, 0.0F, 1.0F),
        .linear_velocity = JPH::Vec3(0.0F, 0.0F, not_a_number),
        .angular_velocity = JPH::Vec3(not_a_number, not_a_number, not_a_number),
    };

    recorder.record(std::vector{still, lost});
    recorder.record(std::vector{lost, still});

    CHECK(recorder.value(aplomb::Metric::nan_count) == 14.0);
}

TEST_CASE("a part whose speed isn't a number leaves max_speed_after_3s to the others", "[runner]") {
    MetricRecorder recorder(1.0F);
    for (int step = 0; step < 3; ++step) {
        recorder.record(
            moving_at({JPH::Vec3(not_a_number, 0.0F, 0.0F), JPH::Vec3(0.0F, 2.0F, 0.0F)}));
    }

    CHECK(recorder.value(aplomb::Metric::max_speed_after_3s) == 2.0);
    CHECK(recorder.value(aplomb::Metric::nan_count) == 3.0);
}
