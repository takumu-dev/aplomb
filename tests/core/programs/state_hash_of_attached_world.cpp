#include "host_physics.hpp"
#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    HostPhysics host;
    auto world = aplomb::World::create(host.world_settings(services));
    [[maybe_unused]] const auto hash = world->state_hash();
}
