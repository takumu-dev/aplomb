#include "step_clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

// Enough for the fastest speed at 30 frames a second and the default step rate.
constexpr float max_steps_per_frame = 8.0F;

} // namespace

StepClock::StepClock(float step_rate) noexcept : step_rate_(step_rate) {}

bool StepClock::paused() const noexcept { return paused_; }

void StepClock::set_paused(bool paused) noexcept {
    paused_ = paused;
    step_asked_ = false;
    owed_steps_ = 0.0F;
}

float StepClock::speed() const noexcept { return speed_; }

void StepClock::set_speed(float speed) noexcept {
    speed_ = std::clamp(speed, min_speed, max_speed);
}

void StepClock::step_once() noexcept { step_asked_ = paused_; }

std::size_t StepClock::advance(float frame_seconds) noexcept {
    if (paused_) {
        const bool step = step_asked_;
        step_asked_ = false;
        return step ? 1 : 0;
    }
    owed_steps_ += std::max(frame_seconds, 0.0F) * speed_ * step_rate_;
    const float steps = std::min(std::floor(owed_steps_), max_steps_per_frame);
    owed_steps_ = std::min(owed_steps_ - steps, 1.0F);
    return static_cast<std::size_t>(steps);
}
