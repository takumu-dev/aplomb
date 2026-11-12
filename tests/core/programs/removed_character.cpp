#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include "rigs.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/world.hpp>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    const auto character = world->add_character(chain_rig(), {});
    world->remove_character(*character);
    [[maybe_unused]] const aplomb::Character& removed = world->character(*character);
}
