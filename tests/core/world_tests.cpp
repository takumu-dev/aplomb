#include <aplomb/core/world.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "recording_log_sink.hpp"
#include "scoped_log_sink.hpp"
#include <aplomb/core/log_sink.hpp>
#include <aplomb/core/object_layers.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <format>
#include <limits>
#include <vector>

namespace {

// Adds a sphere that falls freely from 10 m up, with no damping to slow it, and returns its id.
JPH::BodyID add_falling_sphere(JPH::PhysicsSystem& physics_system) {
    JPH::BodyCreationSettings settings(new JPH::SphereShape(0.5F), JPH::RVec3(0.0F, 10.0F, 0.0F),
                                       JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                       aplomb::object_layers::moving);
    settings.mLinearDamping = 0.0F;
    return physics_system.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

// Ignores Jolt's own assertions for as long as it lives, where Jolt is built with them. Jolt's
// debug build asserts that a step had room, before the world can report that it didn't.
class IgnoredJoltAssertions {
  public:
    IgnoredJoltAssertions() noexcept {
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = [](const char* /*expression*/, const char* /*message*/,
                               const char* /*file*/, JPH::uint /*line*/) { return false; };
#endif
    }

    IgnoredJoltAssertions(const IgnoredJoltAssertions&) = delete;
    IgnoredJoltAssertions(IgnoredJoltAssertions&&) = delete;
    IgnoredJoltAssertions& operator=(const IgnoredJoltAssertions&) = delete;
    IgnoredJoltAssertions& operator=(IgnoredJoltAssertions&&) = delete;

    ~IgnoredJoltAssertions() {
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = previous_;
#endif
    }

  private:
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailedFunction previous_ = JPH::AssertFailed;
#endif
};

// Settings for a world at the given step rate, lent the host's job system and allocator.
aplomb::WorldSettings at_step_rate(HostServices& services, float step_rate) {
    aplomb::WorldSettings settings = services.world_settings();
    settings.step_rate = step_rate;
    return settings;
}

float vertical_velocity(const JPH::PhysicsSystem& physics_system, JPH::BodyID body) {
    return physics_system.GetBodyInterfaceNoLock().GetLinearVelocity(body).GetY();
}

} // namespace

TEST_CASE("an owned world steps its physics system once per step, at 60 Hz by default", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const JPH::BodyID sphere = add_falling_sphere(world->physics_system());

    world->step();
    world->step();
    world->step();

    // Each step adds gravity, 9.81 m/s², for 1/60 s to the sphere's velocity.
    CHECK(vertical_velocity(world->physics_system(), sphere) == Catch::Approx(-0.4905F));
}

TEST_CASE("a world steps at the step rate it is given", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(at_step_rate(services, 120.0F));
    REQUIRE(world.has_value());
    const JPH::BodyID sphere = add_falling_sphere(world->physics_system());

    world->step();
    world->step();
    world->step();

    // Each step adds gravity, 9.81 m/s², for 1/120 s to the sphere's velocity.
    CHECK(vertical_velocity(world->physics_system(), sphere) == Catch::Approx(-0.24525F));
}

TEST_CASE("a world isn't created with a step rate that isn't a positive, finite number", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    // The last rate is so small that one step would last forever.
    const float step_rate = GENERATE(0.0F, -60.0F, std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN(), 1e-40F);

    const auto world = aplomb::World::create(at_step_rate(services, step_rate));

    REQUIRE_FALSE(world.has_value());
    CHECK(world.error().code == aplomb::WorldErrorCode::invalid_step_rate);
}

TEST_CASE("a world error formats as its message", "[core]") {
    const JoltRuntime runtime;
    HostServices services;

    const auto world = aplomb::World::create(at_step_rate(services, 0.0F));

    REQUIRE_FALSE(world.has_value());
    CHECK(std::format("{}", world.error()) ==
          "a world's step rate must be a positive, finite number of steps a second, not 0");
}

TEST_CASE("a step that runs out of room in the physics system logs a warning", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    RecordingLogSink sink;
    const ScopedLogSink scope(sink);
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    // Every pair of 400 spheres in one place touches: 79,800 pairs, more than the physics system
    // makes room for.
    for (int sphere = 0; sphere < 400; ++sphere) {
        add_falling_sphere(world->physics_system());
    }

    {
        const IgnoredJoltAssertions ignored;
        world->step();
    }

    const std::vector<LoggedMessage> messages = sink.messages();
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().severity == aplomb::LogSeverity::warning);
    CHECK_THAT(messages.front().text,
               Catch::Matchers::StartsWith("a step ran out of room in the physics system for "));
}
