#pragma once

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "orbit_camera.hpp"
#include "panels/characters_panel.hpp"
#include "panels/rig_file_panel.hpp"
#include "session.hpp"
#include "step_clock.hpp"

#include <filesystem>

/// Everything the sandbox shows and does, bar the window: the session, the camera, the clock that
/// steps the world, and the panels that control them.
///
/// The camera orbits its target on a drag with the right mouse button, pans on a drag with the
/// middle button or with shift and the right, and zooms on the mouse wheel.
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

    /// Draws the session's world.
    ///
    /// \param draw What to draw with.
    void draw(DebugDraw& draw) const;

    /// Where the camera sees the world from.
    [[nodiscard]] CameraView camera_view() const;

  private:
    void steer_camera();
    void draw_panels();

    Session session_;
    StepClock clock_;
    OrbitCamera camera_;
    RigFilePanel rig_file_;
    CharactersPanel characters_;
};
