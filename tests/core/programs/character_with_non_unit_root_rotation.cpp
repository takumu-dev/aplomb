#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include "rigs.hpp"
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    [[maybe_unused]] const auto character =
        world->add_character(chain_rig(), {.rotation = JPH::Quat(0.0F, 0.0F, 0.0F, 2.0F)});
}
