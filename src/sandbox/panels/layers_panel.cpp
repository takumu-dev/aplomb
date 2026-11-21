#include "panels/layers_panel.hpp"

#include "layers/debug_layers.hpp"

#include <imgui.h>

void draw_layers_panel(DebugLayers& layers) {
    ImGui::Checkbox("Shapes", &layers.shapes);
    ImGui::SameLine();
    ImGui::Checkbox("Joints", &layers.joints);
    ImGui::SameLine();
    ImGui::Checkbox("Joint limits", &layers.joint_limits);
    ImGui::Checkbox("Contacts", &layers.contacts);
    ImGui::SameLine();
    ImGui::Checkbox("Centre of mass", &layers.center_of_mass);
}
