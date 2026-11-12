#include <aplomb/scenes/build_scene.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

TEST_CASE("flat_ground is the one scene there is", "[scenes]") {
    const auto names = aplomb::scene_names();

    CHECK(std::vector(names.begin(), names.end()) == std::vector<std::string_view>{"flat_ground"});
}

TEST_CASE("a scene built by its name adds its bodies to the physics system", "[scenes]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());

    const bool built = aplomb::build_scene("flat_ground", world->physics_system());

    CHECK(built);
    JPH::BodyIDVector bodies;
    world->physics_system().GetBodies(bodies);
    REQUIRE(bodies.size() == 1);
    CHECK(world->physics_system().GetBodyInterface().GetMotionType(bodies.front()) ==
          JPH::EMotionType::Static);
}

TEST_CASE("a name that no scene has builds nothing", "[scenes]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());

    const bool built = aplomb::build_scene("flat ground", world->physics_system());

    CHECK_FALSE(built);
    CHECK(world->physics_system().GetNumBodies() == 0);
}
