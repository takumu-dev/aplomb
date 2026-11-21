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
    host.update(services, 1.0F / 60.0F);
    world->before_physics_step();
}
