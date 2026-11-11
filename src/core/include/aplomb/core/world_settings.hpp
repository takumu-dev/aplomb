/// \file
/// What a world is created from.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>

namespace aplomb {

/// What a world is created from: its step rate, and what the host lends it.
///
/// The host keeps ownership of the job system and the temporary allocator, which must outlive the
/// world. A world never starts threads or allocates scratch memory of its own.
struct WorldSettings {
    /// How many fixed steps the world takes for each second it advances.
    float step_rate = 60.0F;

    /// The job system Jolt runs its work on.
    JPH::JobSystem* job_system = nullptr;

    /// The allocator Jolt takes scratch memory from during a step.
    JPH::TempAllocator* temp_allocator = nullptr;
};

} // namespace aplomb
