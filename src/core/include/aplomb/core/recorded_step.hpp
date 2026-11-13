/// \file
/// A step that a recording world recorded.

#pragma once

#include <aplomb/core/push.hpp>

#include <cstdint>
#include <vector>

namespace aplomb {

/// One step that a world took while recording: the messages it applied at the start of the step,
/// and its state hash after it. Replaying the messages step by step from the same start gives the
/// same state hashes.
struct RecordedStep {
    /// The pushes the step applied, in the order they were sent.
    std::vector<Push> pushes{};

    /// The world's state hash after the step.
    std::uint64_t state_hash = 0;

    /// Whether two recorded steps are the same.
    friend bool operator==(const RecordedStep&, const RecordedStep&) = default;
};

} // namespace aplomb
