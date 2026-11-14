#pragma once

#include "step_clock.hpp"

#include <cstdint>

/// Draws the controls of how the world steps into the current ImGui window: pause and resume, one
/// step while paused, and the speed.
///
/// \param clock The clock the controls change.
/// \param steps_taken How many steps the world has taken.
/// \param step_rate The world's step rate, in steps a second.
void draw_stepping_panel(StepClock& clock, std::uint64_t steps_taken, float step_rate);

/// Takes the stepping controls' keyboard shortcuts, unless ImGui has the keyboard: space pauses and
/// resumes, and the right arrow takes one step while paused.
///
/// \param clock The clock the shortcuts change.
void take_stepping_shortcuts(StepClock& clock);
