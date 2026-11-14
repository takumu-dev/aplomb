#pragma once

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "layers/debug_layers.hpp"
#include "orbit_camera.hpp"
#include "panels/characters_panel.hpp"
#include "panels/replay_panel.hpp"
#include "panels/rig_file_panel.hpp"
#include "panels/rig_inspector_panel.hpp"
#include "push_tool.hpp"
#include "session.hpp"
#include "step_clock.hpp"

#include <filesystem>

/// Everything the sandbox shows and does, bar the window: the session, the camera, the clock that
/// steps the world, the debug layers that draw it, and the panels that control them, record it and
/// play replays back.
///
/// When a replay played back ends, stepping pauses on its last step.
///
/// The camera orbits its target on a drag with the right mouse button, pans on a drag with the
/// middle button or with shift and the right, and zooms on the mouse wheel. A drag with the left
/// button from a part pushes it.
class Sandbox {
  public:
    /// Creates the sandbox, loading the rig file that characters are first added from.
    ///
    /// \param session The session to show.
    /// \param rig_file The rig file's path.
    Sandbox(Session session, const std::filesystem::path& rig_file);

    /// Runs a frame's input and panels through ImGui, inside its frame, then steps the world as
    /// often as the clock says.
    ///
    /// \param frame_seconds How long the last frame took on the wall clock.
    void update(float frame_seconds);

    /// Draws the ground, the session's world through the debug layers that are on, and the push
    /// being dragged.
    ///
    /// \param draw What to draw with.
    void draw(DebugDraw& draw) const;

    /// Where the camera sees the world from.
    [[nodiscard]] CameraView camera_view() const;

  private:
    void steer_camera();
    void use_push_tool();
    void draw_panels();

    Session session_;
    StepClock clock_;
    OrbitCamera camera_;
    DebugLayers layers_;
    PushTool push_tool_;
    RigFilePanel rig_file_;
    CharactersPanel characters_;
    RigInspectorPanel rig_inspector_;
    ReplayPanel replay_;
};
