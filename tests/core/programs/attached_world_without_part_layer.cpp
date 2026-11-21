#include "host_physics.hpp"
#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_settings.hpp>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    HostPhysics host;
    aplomb::WorldSettings settings = host.world_settings(services);
    settings.part_layer.reset();
    [[maybe_unused]] const auto world = aplomb::World::create(settings);
}
