#pragma once

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/transform.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Math/Vec3.h>

#include <expected>
#include <memory>
#include <string>

/// The raylib backend of `DebugDraw`: the sandbox's window, on OpenGL 3.3 core, with Dear ImGui
/// set up on it through rlImGui.
///
/// A frame goes `begin_frame()`, the frame's ImGui calls, `begin_scene()`, its drawing, then
/// `end_scene()` and `end_frame()`. Lines are gathered over the scene and drawn at its end from one
/// vertex buffer, the overlay lines last and without the depth test; shapes are drawn as they come,
/// lit by a fixed light. There is one window, so one
/// backend, at a time.
///
/// A moved-from backend may only be destroyed or assigned to; anything else fires an assertion.
class RaylibDebugDraw final : public DebugDraw {
  public:
    /// Opens the window and sets Dear ImGui up on it.
    ///
    /// \param title The window's title.
    /// \return The backend, or why the window couldn't be opened.
    [[nodiscard]] static std::expected<RaylibDebugDraw, std::string> open(const std::string& title);

    /// Moves a backend, leaving `other` empty.
    RaylibDebugDraw(RaylibDebugDraw&& other) noexcept;

    /// Moves a backend into this one, closing the window this one had.
    RaylibDebugDraw& operator=(RaylibDebugDraw&& other) noexcept;

    RaylibDebugDraw(const RaylibDebugDraw&) = delete;
    RaylibDebugDraw& operator=(const RaylibDebugDraw&) = delete;

    /// Shuts Dear ImGui down and closes the window.
    ~RaylibDebugDraw() override;

    /// Whether the owner has asked to close the window.
    [[nodiscard]] bool closing() const;

    /// How long the last frame took, in seconds.
    [[nodiscard]] float frame_seconds() const;

    /// Begins a frame: clears the window, and begins Dear ImGui's frame.
    void begin_frame();

    /// Begins drawing the scene, seen as the view says.
    ///
    /// \param view Where the scene is seen from.
    void begin_scene(const CameraView& view);

    /// Ends drawing the scene, drawing the lines and overlay lines gathered since `begin_scene()`.
    void end_scene();

    /// Ends the frame: draws Dear ImGui's windows over the scene, and shows the frame.
    void end_frame();

    void line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) override;
    void overlay_line(JPH::Vec3 from, JPH::Vec3 to, JPH::Color color) override;
    void box(const aplomb::Transform& transform, JPH::Vec3 half_extents, JPH::Color color) override;
    void capsule(const aplomb::Transform& transform, float radius, float half_height,
                 JPH::Color color) override;
    void sphere(const aplomb::Transform& transform, float radius, JPH::Color color) override;

  private:
    struct Resources;

    explicit RaylibDebugDraw(std::unique_ptr<Resources> resources) noexcept;

    // Fires an assertion if the backend has been moved from.
    void assert_not_moved_from() const;

    // The backend's resources, which a moved-from backend no longer has.
    [[nodiscard]] Resources& resources() const;

    std::unique_ptr<Resources> resources_;
};
