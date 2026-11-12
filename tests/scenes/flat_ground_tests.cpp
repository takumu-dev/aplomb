#include <aplomb/scenes/flat_ground.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include <aplomb/core/object_layers.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("flat ground is a static body on the scene's layer, with high friction", "[scenes]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());

    const JPH::BodyID ground = aplomb::build_flat_ground(world->physics_system());

    const JPH::BodyInterface& bodies = world->physics_system().GetBodyInterface();
    CHECK(bodies.IsAdded(ground));
    CHECK(bodies.GetMotionType(ground) == JPH::EMotionType::Static);
    CHECK(bodies.GetObjectLayer(ground) == aplomb::object_layers::scene);
    CHECK(bodies.GetFriction(ground) >= 1.0F);
}

TEST_CASE("a sphere dropped onto flat ground comes to rest on its surface, at a height of 0",
          "[scenes]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    aplomb::build_flat_ground(world->physics_system());
    JPH::BodyInterface& bodies = world->physics_system().GetBodyInterface();
    const JPH::BodyID sphere = bodies.CreateAndAddBody(
        JPH::BodyCreationSettings(new JPH::SphereShape(0.5F), JPH::RVec3(3.0F, 2.0F, -4.0F),
                                  JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                  aplomb::object_layers::moving),
        JPH::EActivation::Activate);

    for (int step = 0; step < 180; ++step) {
        world->step();
    }

    // Jolt lets a body rest up to 2 cm into another, its default penetration slop.
    CHECK(bodies.GetPosition(sphere).GetY() == Catch::Approx(0.5F).margin(0.02F));
    CHECK(bodies.GetLinearVelocity(sphere).Length() == Catch::Approx(0.0F).margin(0.01F));
}
