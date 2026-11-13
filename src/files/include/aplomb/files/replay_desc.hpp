/// \file
/// A replay, as a replay file describes it.

#pragma once

#include <aplomb/core/recorded_step.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace aplomb {

/// How a replay's world starts, before its first step.
struct ReplayInitialState {
    /// The characters in the world, each made from the replay's rig and added in this order, so
    /// that each one's id is its place here.
    std::vector<InitialState> characters{};

    /// The world's state hash once its scene is built and its characters are added.
    std::uint64_t state_hash = 0;
};

/// A session's messages, step by step, that reproduce the session exactly: a world at a step rate,
/// with a scene and characters made from one rig, and each step's messages and the state hash the
/// world had after it.
///
/// Every number reads back from a replay file with the bits it was written with, so a replay plays
/// back exactly what was recorded.
struct ReplayDesc {
    /// The version of aplomb that recorded the replay.
    std::string aplomb_version{};

    /// The version of Jolt that the aplomb that recorded the replay was built with.
    std::string jolt_version{};

    /// The hash of the rig file the characters were made from, as `rig_file_hash` gives it.
    std::uint64_t rig_file_hash = 0;

    /// The world's step rate, in steps a second.
    float step_rate = 0.0F;

    /// The seed the world's random generators are derived from.
    std::uint64_t seed = 0;

    /// The name of the scene built into the world, such as `flat_ground`.
    std::string scene{};

    /// How the world starts.
    ReplayInitialState initial{};

    /// Each step the world took, in order. Each push is to one of the initial state's characters.
    std::vector<RecordedStep> steps{};
};

} // namespace aplomb
