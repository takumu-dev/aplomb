// A host's program that uses both of aplomb's libraries: it reads a rig file, adds a character made
// from it to a world that owns its physics system, steps the world for a second and prints the
// world's state hash. It sets Jolt up itself, as a host does, rather than with the tests' support,
// so that it links nothing a host doesn't.
//
//   aplomb_host_project <rig file>

#include <aplomb/core/version.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/files/rig_file.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/RegisterTypes.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <span>

namespace {

constexpr int worker_threads = 2;
constexpr JPH::uint temp_allocator_bytes = 10 * 1024 * 1024;
constexpr int steps = 60;

// Steps a world with a character made from the rig, once Jolt is set up.
int run(const aplomb::RigDesc& rig) {
    JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                        worker_threads);
    JPH::TempAllocatorImpl temp_allocator(temp_allocator_bytes);
    auto world =
        aplomb::World::create({.job_system = &job_system, .temp_allocator = &temp_allocator});
    if (!world) {
        std::println(stderr, "the world couldn't be created: {}", world.error());
        return EXIT_FAILURE;
    }
    if (auto character = world->add_character(rig, {.position = JPH::Vec3(0.0F, 1.0F, 0.0F)});
        !character) {
        std::println(stderr, "the character couldn't be added: {}", character.error());
        return EXIT_FAILURE;
    }
    for (int step = 0; step < steps; ++step) {
        world->step();
    }
    std::println("aplomb {} on Jolt {}: state hash {:016x}", aplomb::version(),
                 aplomb::jolt_version(), world->state_hash());
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 2) {
        std::println(stderr, "usage: aplomb_host_project <rig file>");
        return EXIT_FAILURE;
    }
    const auto rig = aplomb::load_rig_file(arguments[1]);
    if (!rig) {
        std::println(stderr, "{}", rig.error());
        return EXIT_FAILURE;
    }

    JPH::RegisterDefaultAllocator();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Jolt's factory is a raw global
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    const int result = run(*rig);
    JPH::UnregisterTypes();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Jolt's factory is a raw global
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    return result;
}
