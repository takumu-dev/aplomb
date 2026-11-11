#include "host_services.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSettings.h>

namespace {

constexpr int worker_threads = 2;
constexpr JPH::uint temp_allocator_bytes = 10 * 1024 * 1024;

} // namespace

HostServices::HostServices()
    : job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_threads),
      temp_allocator_(temp_allocator_bytes) {}
