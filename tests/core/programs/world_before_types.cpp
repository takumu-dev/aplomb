#include "host_services.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>

void break_precondition() {
    JPH::RegisterDefaultAllocator();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Jolt's factory is a raw global
    JPH::Factory::sInstance = new JPH::Factory();
    HostServices services;
    [[maybe_unused]] const auto world = aplomb::World::create(services.world_settings());
}
