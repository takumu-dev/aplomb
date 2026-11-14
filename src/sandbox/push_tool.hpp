#pragma once

#include "debug_draw/camera_view.hpp"
#include "debug_draw/debug_draw.hpp"
#include "session.hpp"
#include <aplomb/core/typed_id.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

#include <optional>

/// Pushes a part of a character by hand: the owner grabs the part under the cursor, drags from it
/// and lets go, and the part is pushed the way of the drag, as hard as the drag is long.
///
/// The drag is measured on the plane through the grabbed point that faces the camera, from the
/// grabbed point to where the cursor's ray meets that plane. The grabbed point stays on its part,
/// moving with it while the world steps. Letting go sends a push, which the world applies at the
/// start of its next step, so that a push given while paused takes effect on the next single step.
class PushTool {
  public:
    /// The least impulse a metre of drag gives, in newton seconds.
    static constexpr float min_impulse_per_meter = 5.0F;

    /// The greatest impulse a metre of drag gives, in newton seconds.
    static constexpr float max_impulse_per_meter = 500.0F;

    /// Grabs the part under the cursor, if the first thing the cursor's ray meets is a part of one
    /// of the session's characters, and starts a drag from where the ray meets it.
    ///
    /// \param session The session whose characters can be grabbed.
    /// \param view Where the world is seen from.
    /// \param direction The way from the eye through the cursor, as a unit vector.
    void grab(const Session& session, const CameraView& view, JPH::Vec3 direction);

    /// Moves the end of the drag to where the cursor is.
    ///
    /// \param view Where the world is seen from.
    /// \param direction The way from the eye through the cursor, as a unit vector.
    void aim(const CameraView& view, JPH::Vec3 direction);

    /// Lets go of the grabbed part, pushing it with the drag's impulse at the grabbed point. A drag
    /// of no length sends no push.
    ///
    /// \param session The session the grab was made in.
    void release(Session& session);

    /// Lets go of the grabbed part without pushing it, as when the session it was grabbed in is
    /// replaced.
    void let_go() noexcept;

    /// The impulse the drag gives now, while a part is grabbed.
    ///
    /// \param session The session the grab was made in.
    /// \return The impulse, in newton seconds, or none when no part is grabbed or its character
    /// has been removed.
    [[nodiscard]] std::optional<JPH::Vec3> impulse(const Session& session) const;

    /// Draws the drag, while a part is grabbed: a mark at the grabbed point, and an arrow from it
    /// the way the push will go, as long as the drag.
    ///
    /// \param session The session the grab was made in.
    /// \param draw What to draw with.
    void draw(const Session& session, DebugDraw& draw) const;

    /// The impulse each metre of drag gives, in newton seconds.
    [[nodiscard]] float impulse_per_meter() const noexcept;

    /// Sets the impulse each metre of drag gives, held between `min_impulse_per_meter` and
    /// `max_impulse_per_meter`.
    ///
    /// \param impulse_per_meter The impulse, in newton seconds.
    void set_impulse_per_meter(float impulse_per_meter) noexcept;

  private:
    struct Grab {
        aplomb::CharacterId character;
        aplomb::PartId part;
        // The grabbed point, relative to the part's body.
        JPH::Vec3 on_part;
    };

    // Where the grabbed point is now, and the drag from it, while a part of a character that is
    // still in the session is grabbed.
    struct Drag {
        JPH::Vec3 from;
        JPH::Vec3 offset;
    };

    [[nodiscard]] std::optional<Drag> drag(const Session& session) const;

    std::optional<Grab> grab_;
    CameraView view_;
    JPH::Vec3 direction_ = JPH::Vec3::sZero();
    float impulse_per_meter_ = 60.0F;
};
