#include <aplomb/core/world.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "recording_log_sink.hpp"
#include "rigs.hpp"
#include "scoped_log_sink.hpp"
#include <aplomb/core/log_sink.hpp>
#include <aplomb/core/object_layers.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <format>
#include <limits>
#include <utility>
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

const aplomb::PartId pelvis(0);

// A world with the chain rig in mid-air, falling freely, as its only character.
struct ChainInAir {
    aplomb::World world;
    aplomb::CharacterId character;
};

ChainInAir chain_in_air(HostServices& services) {
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const auto character = world->add_character(chain_rig(), {});
    REQUIRE(character.has_value());
    return {.world = std::move(*world), .character = *character};
}

JPH::Vec3 pelvis_position(const ChainInAir& chain) {
    return chain.world.character(chain.character).physics_pose()[pelvis].position;
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

TEST_CASE("a world's state hash stays the same while its state does", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    add_falling_sphere(world->physics_system());

    const std::uint64_t before = world->state_hash();

    CHECK(world->state_hash() == before);
}

TEST_CASE("a world's state hash changes when a step moves a body", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    add_falling_sphere(world->physics_system());
    const std::uint64_t before = world->state_hash();

    world->step();

    CHECK(world->state_hash() != before);
}

TEST_CASE("a world's state hash covers its scene", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const JPH::BodyID ground = aplomb::build_flat_ground(world->physics_system());
    const std::uint64_t before = world->state_hash();

    world->physics_system().GetBodyInterface().SetPosition(ground, JPH::RVec3(5.0F, -1.0F, 0.0F),
                                                           JPH::EActivation::DontActivate);

    CHECK(world->state_hash() != before);
}

TEST_CASE("a world's state hash covers its characters' joints", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    REQUIRE(world->add_character(chain_rig(), {}).has_value());
    const JPH::Constraints joints = world->physics_system().GetConstraints();
    REQUIRE_FALSE(joints.empty());
    const std::uint64_t before = world->state_hash();

    joints.front()->SetEnabled(false);

    CHECK(world->state_hash() != before);
}

TEST_CASE("two worlds made alike give the same state hash at every step", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    const auto make_world = [&] {
        auto world = aplomb::World::create(services.world_settings());
        REQUIRE(world.has_value());
        aplomb::build_flat_ground(world->physics_system());
        REQUIRE(world->add_character(chain_rig(), {.position = JPH::Vec3(0.0F, 0.5F, 0.0F)})
                    .has_value());
        return std::move(*world);
    };
    aplomb::World first = make_world();
    aplomb::World second = make_world();

    // Two seconds at 60 Hz: the character falls, lands and folds onto the ground.
    for (int step = 0; step < 120; ++step) {
        first.step();
        second.step();
        REQUIRE(first.state_hash() == second.state_hash());
    }
}

TEST_CASE("a pushed part moves in its impulse's direction", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    ChainInAir pushed = chain_in_air(services);
    ChainInAir unpushed = chain_in_air(services);
    const JPH::Vec3 impulse(20.0F, 0.0F, 0.0F);

    pushed.world.push(pushed.character, pelvis, pelvis_position(pushed), impulse);
    pushed.world.step();
    unpushed.world.step();

    // Both fall alike, so the push alone parts them.
    const JPH::Vec3 moved = pelvis_position(pushed) - pelvis_position(unpushed);
    CHECK(moved.Length() > 0.0F);
    CHECK(moved.Normalized().Dot(impulse.Normalized()) > 0.99F);
}

TEST_CASE("a push takes effect at the next step, not when it is sent", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    ChainInAir pushed = chain_in_air(services);
    ChainInAir unpushed = chain_in_air(services);
    REQUIRE(pushed.world.state_hash() == unpushed.world.state_hash());

    pushed.world.push(pushed.character, pelvis, pelvis_position(pushed),
                      JPH::Vec3(20.0F, 0.0F, 0.0F));

    CHECK(pushed.world.state_hash() == unpushed.world.state_hash());
    pushed.world.step();
    unpushed.world.step();
    CHECK(pushed.world.state_hash() != unpushed.world.state_hash());
}

TEST_CASE("a push is applied at its point, so one above a part's centre turns the part more",
          "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    ChainInAir at_centre = chain_in_air(services);
    ChainInAir above_centre = chain_in_air(services);
    const JPH::Vec3 impulse(20.0F, 0.0F, 0.0F);
    const auto pelvis_spin = [](const ChainInAir& chain) {
        const JPH::BodyID body = chain.world.character(chain.character).part_body(pelvis);
        return chain.world.physics_system().GetBodyInterface().GetAngularVelocity(body).GetZ();
    };

    at_centre.world.push(at_centre.character, pelvis, pelvis_position(at_centre), impulse);
    above_centre.world.push(above_centre.character, pelvis,
                            pelvis_position(above_centre) + JPH::Vec3(0.0F, 0.1F, 0.0F), impulse);
    at_centre.world.step();
    above_centre.world.step();

    // An impulse along +X above the centre turns the part about -Z, besides how the thigh turns
    // it by holding back its joint.
    CHECK(pelvis_spin(above_centre) < pelvis_spin(at_centre) - 1.0F);
}

TEST_CASE("the same pushes sent in the same order give the same state hash at every step",
          "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    ChainInAir first = chain_in_air(services);
    ChainInAir second = chain_in_air(services);
    const aplomb::PartId shin(2);
    // Two pushes before one step, then one more some steps later.
    const auto push = [&](ChainInAir& chain, int step) {
        if (step == 0) {
            chain.world.push(chain.character, pelvis, pelvis_position(chain),
                             JPH::Vec3(15.0F, 0.0F, 5.0F));
            chain.world.push(chain.character, shin, JPH::Vec3(0.0F, 0.3F, 0.05F),
                             JPH::Vec3(0.0F, 2.0F, -8.0F));
        } else if (step == 30) {
            chain.world.push(chain.character, shin, JPH::Vec3(0.1F, 0.0F, 0.0F),
                             JPH::Vec3(-6.0F, 0.0F, 0.0F));
        }
    };

    for (int step = 0; step < 60; ++step) {
        push(first, step);
        push(second, step);
        first.world.step();
        second.world.step();
        REQUIRE(first.world.state_hash() == second.world.state_hash());
    }
}

TEST_CASE("a push to a character removed before the next step is dropped", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    ChainInAir chain = chain_in_air(services);
    const auto other = chain.world.add_character(chain_rig(), {});
    REQUIRE(other.has_value());
    ChainInAir unpushed = chain_in_air(services);
    REQUIRE(unpushed.world.add_character(chain_rig(), {}).has_value());
    unpushed.world.remove_character(unpushed.character);

    chain.world.push(chain.character, pelvis, pelvis_position(chain), JPH::Vec3(20.0F, 0.0F, 0.0F));
    chain.world.remove_character(chain.character);
    chain.world.step();
    unpushed.world.step();

    CHECK(chain.world.state_hash() == unpushed.world.state_hash());
}
