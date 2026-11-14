#include "panels/push_panel.hpp"

#include "push_tool.hpp"

#include <imgui.h>

void draw_push_panel(PushTool& tool) {
    float impulse_per_meter = tool.impulse_per_meter();
    if (ImGui::SliderFloat("Impulse", &impulse_per_meter, PushTool::min_impulse_per_meter,
                           PushTool::max_impulse_per_meter, "%.0f N s per metre",
                           ImGuiSliderFlags_Logarithmic)) {
        tool.set_impulse_per_meter(impulse_per_meter);
    }
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted("Left drag from a part: push it that way, harder the further the drag. "
                           "A push while paused waits for the next step.");
    ImGui::PopTextWrapPos();
}
