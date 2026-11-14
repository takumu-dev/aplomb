#include "sandbox.hpp"

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "layers/center_of_mass_layer.hpp"
#include "layers/contacts_layer.hpp"
#include "layers/ground.hpp"
#include "layers/joint_limits_layer.hpp"
#include "layers/joints_layer.hpp"
#include "layers/shapes_layer.hpp"
#include "panels/layers_panel.hpp"
#include "panels/push_panel.hpp"
#include "panels/stepping_panel.hpp"
#include "push_tool.hpp"
#include "session.hpp"
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <imgui.h>

#include <cfloat>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
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
    use_push_tool();
    for (std::size_t steps = clock_.advance(frame_seconds); steps > 0; --steps) {
        const bool playing = session_.playing();
        session_.step();
        if (playing && !session_.playing()) {
            clock_.set_paused(true);
            break;
        }
    }
}

void Sandbox::draw(DebugDraw& draw) const {
    draw_ground(draw);
    const aplomb::World& world = session_.world();
    if (layers_.shapes) {
        draw_shapes_layer(world, session_.characters(), draw);
    }
    if (layers_.joint_limits) {
        draw_joint_limits_layer(world, session_.characters(), draw);
    }
    if (layers_.joints) {
        draw_joints_layer(world, session_.characters(), draw);
    }
    if (layers_.contacts) {
        draw_contacts_layer(session_.contacts(), draw);
    }
    if (layers_.center_of_mass) {
        draw_center_of_mass_layer(world, session_.characters(), draw);
    }
    push_tool_.draw(session_, draw);
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

void Sandbox::use_push_tool() {
    const ImGuiIO& io = ImGui::GetIO();
    // The drag is aimed before it is let go of, so that a push goes where the cursor is now.
    if (ImGui::IsMousePosValid() && io.DisplaySize.x > 0.0F && io.DisplaySize.y > 0.0F) {
        const CameraView view = camera_.view();
        const JPH::Vec3 direction = view.direction_through(
            (2.0F * io.MousePos.x / io.DisplaySize.x) - 1.0F,
            1.0F - (2.0F * io.MousePos.y / io.DisplaySize.y), io.DisplaySize.x / io.DisplaySize.y);
        // A session playing a replay back takes no pushes but the replay's.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse &&
            !session_.playing()) {
            push_tool_.grab(session_, view, direction);
        } else {
            push_tool_.aim(view, direction);
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        push_tool_.release(session_);
    }
    const std::optional<JPH::Vec3> impulse = push_tool_.impulse(session_);
    if (impulse) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(std::format("{:.1f} N s", impulse->Length()).c_str());
        ImGui::EndTooltip();
    }
}

void Sandbox::draw_panels() {
    ImGui::SetNextWindowPos({panel_margin, panel_margin}, ImGuiCond_FirstUseEver);
    // The panel grows with the list of characters, at a width that leaves its widgets room.
    ImGui::SetNextWindowSizeConstraints({panel_width, 0.0F}, {panel_width, FLT_MAX});
    if (ImGui::Begin("Sandbox", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(std::format("{:.0f} FPS", ImGui::GetIO().Framerate).c_str());
        if (ImGui::CollapsingHeader("Stepping", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_stepping_panel(clock_, session_.steps_taken(), session_.step_rate());
        }
        if (ImGui::CollapsingHeader("Rig file", ImGuiTreeNodeFlags_DefaultOpen)) {
            rig_file_.draw();
        }
        if (ImGui::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen)) {
            characters_.draw(session_, rig_file_.rig());
        }
        if (ImGui::CollapsingHeader("Replay", ImGuiTreeNodeFlags_DefaultOpen) &&
            replay_.draw(session_, rig_file_)) {
            // A part grabbed in the session replaced belongs to none of the new one's characters,
            // and a replay played back may step at another rate.
            push_tool_.let_go();
            clock_.set_step_rate(session_.step_rate());
        }
        if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_layers_panel(layers_);
        }
        if (ImGui::CollapsingHeader("Push", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_push_panel(push_tool_);
        }
        if (ImGui::CollapsingHeader("Camera")) {
            ImGui::TextUnformatted("Right drag: orbit\nMiddle drag, or shift and right drag: pan\n"
                                   "Wheel: zoom");
        }
    }
    ImGui::End();
    rig_inspector_.draw(session_);
}
