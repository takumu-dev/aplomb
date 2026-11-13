#include "panels/stepping_panel.hpp"

#include "step_clock.hpp"

#include <imgui.h>

#include <cstdint>
#include <format>

void draw_stepping_panel(StepClock& clock, std::uint64_t steps_taken, float step_rate) {
    if (ImGui::Button(clock.paused() ? "Resume" : "Pause")) {
        clock.set_paused(!clock.paused());
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!clock.paused());
    if (ImGui::Button("Step")) {
        clock.step_once();
    }
    ImGui::EndDisabled();

    float speed = clock.speed();
    if (ImGui::SliderFloat("Speed", &speed, StepClock::min_speed, StepClock::max_speed, "%.2fx",
                           ImGuiSliderFlags_Logarithmic)) {
        clock.set_speed(speed);
    }
    ImGui::SameLine();
    if (ImGui::Button("1x")) {
        clock.set_speed(1.0F);
    }

    ImGui::TextUnformatted(
        std::format("Step {}, {:.2f} s at {:g} Hz", steps_taken,
                    static_cast<double>(steps_taken) / static_cast<double>(step_rate), step_rate)
            .c_str());
}

void take_stepping_shortcuts(StepClock& clock) {
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        clock.set_paused(!clock.paused());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        clock.step_once();
    }
}
