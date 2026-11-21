#include "host_services.hpp"

#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSettings.h>

namespace {

// The scenario runner's number of threads, which the golden hashes are recorded with, so that the
// sandbox's world steps as the runner's does.
constexpr int worker_threads = 2;
constexpr JPH::uint temp_allocator_bytes = 10 * 1024 * 1024;

} // namespace

HostServices::HostServices()
    : job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_threads),
      temp_allocator_(temp_allocator_bytes) {}

aplomb::WorldSettings HostServices::world_settings() noexcept {
    return {.job_system = &job_system_, .temp_allocator = &temp_allocator_};
}
