#include "sandbox.hpp"

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "layers/shapes_layer.hpp"
#include "panels/stepping_panel.hpp"
#include "session.hpp"

#include <imgui.h>

#include <cfloat>
#include <cstddef>
#include <filesystem>
#include <utility>

namespace {

// How far the camera turns, and how far it pans as a fraction of its distance, for each pixel the
// mouse is dragged.
constexpr float orbit_per_pixel = 0.008F;
constexpr float pan_per_pixel = 0.0015F;

constexpr float panel_margin = 10.0F;
constexpr float panel_width = 380.0F;

} // namespace

Sandbox::Sandbox(Session session, const std::filesystem::path& rig_file)
    : session_(std::move(session)), clock_(session_.step_rate()), rig_file_(rig_file) {}

void Sandbox::update(float frame_seconds) {
    steer_camera();
    take_stepping_shortcuts(clock_);
    draw_panels();
    for (std::size_t steps = clock_.advance(frame_seconds); steps > 0; --steps) {
        session_.step();
    }
}

void Sandbox::draw(DebugDraw& draw) const {
    draw_shapes_layer(session_.world(), session_.characters(), draw);
}

CameraView Sandbox::camera_view() const { return camera_.view(); }

void Sandbox::steer_camera() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }
    const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || (right && io.KeyShift)) {
        camera_.pan(-io.MouseDelta.x * pan_per_pixel, io.MouseDelta.y * pan_per_pixel);
    } else if (right) {
        camera_.orbit(-io.MouseDelta.x * orbit_per_pixel, io.MouseDelta.y * orbit_per_pixel);
    }
    camera_.zoom(io.MouseWheel);
}

void Sandbox::draw_panels() {
    ImGui::SetNextWindowPos({panel_margin, panel_margin}, ImGuiCond_FirstUseEver);
    // The panel grows with the list of characters, at a width that leaves its widgets room.
    ImGui::SetNextWindowSizeConstraints({panel_width, 0.0F}, {panel_width, FLT_MAX});
    if (ImGui::Begin("Sandbox", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::CollapsingHeader("Stepping", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_stepping_panel(clock_, session_.steps_taken(), session_.step_rate());
        }
        if (ImGui::CollapsingHeader("Rig file", ImGuiTreeNodeFlags_DefaultOpen)) {
            rig_file_.draw();
        }
        if (ImGui::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen)) {
            characters_.draw(session_, rig_file_.rig());
        }
        if (ImGui::CollapsingHeader("Camera")) {
            ImGui::TextUnformatted("Right drag: orbit\nMiddle drag, or shift and right drag: pan\n"
                                   "Wheel: zoom");
        }
    }
    ImGui::End();
}
