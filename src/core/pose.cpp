#include <aplomb/core/pose.hpp>

#include <aplomb/core/assertion.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace aplomb {

Pose::Pose(std::vector<Transform> transforms) noexcept : transforms_(std::move(transforms)) {}

std::size_t Pose::part_count() const noexcept { return transforms_.size(); }

const Transform& Pose::operator[](PartId part) const {
    APLOMB_ASSERT(part.index() < transforms_.size(), "the part is one of the pose's parts");
    return transforms_[part.index()];
}

} // namespace aplomb
