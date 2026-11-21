#pragma once

#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

/// What a host lends a world: a job system with a fixed number of threads, so that results don't
/// depend on the machine, and a temporary allocator. Jolt's allocator must be registered first.
class HostServices {
  public:
    HostServices();

    /// The job system, with two worker threads.
    JPH::JobSystem& job_system() noexcept { return job_system_; }

    /// The temporary allocator.
    JPH::TempAllocator& temp_allocator() noexcept { return temp_allocator_; }

    /// Settings for a world at the default step rate, lent this job system and allocator.
    aplomb::WorldSettings world_settings() noexcept {
        return {.job_system = &job_system_, .temp_allocator = &temp_allocator_};
    }

  private:
    JPH::JobSystemThreadPool job_system_;
    JPH::TempAllocatorImpl temp_allocator_;
};
