#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include "rigs.hpp"
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    const auto character = world->add_character(chain_rig(), {});
    // The chain rig has three parts.
    world->push(*character, aplomb::PartId(3), JPH::Vec3::sZero(), JPH::Vec3::sAxisX());
}
