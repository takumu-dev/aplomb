#pragma once

#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

/// What the sandbox lends its world, as a host does: a job system and a temporary allocator. Jolt's
/// allocator must be registered first.
class HostServices {
  public:
    HostServices();

    /// Settings for a world at the default step rate, lent this job system and allocator.
    [[nodiscard]] aplomb::WorldSettings world_settings() noexcept;

  private:
    JPH::JobSystemThreadPool job_system_;
    JPH::TempAllocatorImpl temp_allocator_;
};
