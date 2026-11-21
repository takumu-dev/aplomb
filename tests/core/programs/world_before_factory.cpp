#include "host_services.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

void break_precondition() {
    JPH::RegisterDefaultAllocator();
    HostServices services;
    [[maybe_unused]] const auto world = aplomb::World::create(services.world_settings());
}
