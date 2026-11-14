#pragma once

#include <aplomb/files/metrics_file.hpp>

#include <cstddef>
#include <optional>

/// How far a session has played its replay back, and whether its state hashes were the replay's.
struct Playback {
    /// How many steps the replay has.
    std::size_t steps = 0;

    /// How many of them the session has played back.
    std::size_t played = 0;

    /// The first step at which the session's state hash wasn't the replay's, with the replay's
    /// first, or none while every one has been. Step 0 is before the first step.
    std::optional<aplomb::StateHashDifference> first_differing_step{};

    /// Whether every step of the replay has been played back.
    [[nodiscard]] bool finished() const noexcept { return played == steps; }
};
