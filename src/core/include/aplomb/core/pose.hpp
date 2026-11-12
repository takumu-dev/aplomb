/// \file
/// A pose: a transform for each part of a rig.

#pragma once

#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>

#include <cstddef>
#include <vector>

namespace aplomb {

/// A transform for each part of a rig, relative to its parent part, with the root part's relative
/// to the world.
class Pose {
  public:
    /// Creates a pose of no parts.
    Pose() = default;

    /// Creates a pose from a transform for each part, in the order of the rig's parts.
    ///
    /// \param transforms The transform of each part, relative to its parent part, or the root
    /// part's relative to the world.
    explicit Pose(std::vector<Transform> transforms) noexcept;

    /// How many parts the pose has a transform for.
    [[nodiscard]] std::size_t part_count() const noexcept;

    /// The transform of a part, relative to its parent part, or the root part's relative to the
    /// world.
    ///
    /// \param part A part of the pose.
    [[nodiscard]] const Transform& operator[](PartId part) const;

  private:
    std::vector<Transform> transforms_;
};

} // namespace aplomb
