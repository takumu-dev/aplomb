#pragma once

#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/typed_id.hpp>

#include <expected>
#include <optional>
#include <vector>

namespace aplomb::detail {

/// How the parts of a rig with no problems hang together.
struct RigStructure {
    /// Each part's parent part, or none for the root part, in the order of the rig's parts.
    std::vector<std::optional<PartId>> parents;

    /// Every part, each after its parent part, beginning with the root part.
    std::vector<PartId> parents_first;
};

/// Checks a rig in full.
///
/// \param rig The rig to check.
/// \return How its parts hang together, or every problem it has.
[[nodiscard]] std::expected<RigStructure, std::vector<RigProblem>> check_rig(const RigDesc& rig);

} // namespace aplomb::detail
