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
    // The world steps at 60 Hz, and the host at 30 Hz.
    world->before_physics_step();
    host.update(services, 1.0F / 30.0F);
    world->after_physics_step();
}
