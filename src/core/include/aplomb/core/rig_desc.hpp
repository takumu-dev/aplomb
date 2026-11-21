/// \file
/// A rig, as a host describes it.

#pragma once

#include <aplomb/core/part_desc.hpp>

#include <string>
#include <vector>

namespace aplomb {

/// The physical description of a character: its parts, and the joints and muscles between them.
///
/// A rig has exactly one root part, and every other part's parent chain leads to it. A world
/// checks a rig in full before it makes a character from it.
struct RigDesc {
    /// The rig's name.
    std::string name{};

    /// The rig's parts, in any order. A part's `PartId` is its place here.
    std::vector<PartDesc> parts{};
};

} // namespace aplomb
