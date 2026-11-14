#pragma once

#include <cstddef>

/// When the sandbox steps its world: how many fixed steps each frame takes, from the time the
/// frame took, the speed, and whether stepping is paused.
///
/// Running, the world keeps pace with the wall clock scaled by the speed, so that at a speed of 1 a
/// second of the world takes a second. Paused, it takes only the single steps asked for.
class StepClock {
  public:
    /// The slowest speed there is.
    static constexpr float min_speed = 0.05F;

    /// The fastest speed there is.
    static constexpr float max_speed = 4.0F;

    /// Creates a clock that runs at a speed of 1.
    ///
    /// \param step_rate The world's step rate, in steps a second.
    explicit StepClock(float step_rate) noexcept;

    /// Whether stepping is paused.
    [[nodiscard]] bool paused() const noexcept;

    /// Pauses or resumes stepping. Resuming starts afresh from the next frame, rather than
    /// catching up on the time spent paused.
    ///
    /// \param paused Whether to pause it.
    void set_paused(bool paused) noexcept;

    /// How many seconds of the world pass for each second of the wall clock.
    [[nodiscard]] float speed() const noexcept;

    /// Sets the speed, held between `min_speed` and `max_speed`.
    ///
    /// \param speed How many seconds of the world pass for each second of the wall clock.
    void set_speed(float speed) noexcept;

    /// Asks for one step while paused, which the next frame takes. It does nothing while running.
    void step_once() noexcept;

    /// Advances the clock by a frame.
    ///
    /// \param frame_seconds How long the frame took on the wall clock.
    /// \return How many steps the world takes this frame. A frame never takes more than a few, so
    /// that a slow frame or a stall doesn't leave the world ever further behind; the time it
    /// can't make up is dropped.
    [[nodiscard]] std::size_t advance(float frame_seconds) noexcept;

  private:
    float step_rate_;
    float speed_ = 1.0F;
    bool paused_ = false;
    bool step_asked_ = false;
    // The steps owed but not yet taken, as a fraction of a step.
    float owed_steps_ = 0.0F;
};
