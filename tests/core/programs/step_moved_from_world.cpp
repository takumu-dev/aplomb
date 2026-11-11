#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "precondition_test.hpp"
#include <aplomb/core/world.hpp>

#include <utility>

void break_precondition() {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    [[maybe_unused]] const aplomb::World moved_to = std::move(*world);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): the precondition
    world->step();
}
