#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_settings.hpp>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::WorldSettings settings = services.world_settings();
    settings.temp_allocator = nullptr;
    [[maybe_unused]] const auto world = aplomb::World::create(settings);
}
