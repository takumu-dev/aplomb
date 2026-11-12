#include <aplomb/core/character.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "required.hpp"
#include "rigs.hpp"
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/pose.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

constexpr float close_enough_squared = 1e-10F;

const aplomb::PartId pelvis(0);
const aplomb::PartId thigh(1);
const aplomb::PartId shin(2);

// The priority of the constraint that joins a part, whose body is given, to its parent part. Jolt
// makes the parent part's body a joint's first, and the part's its second.
JPH::uint32 priority_of_joint_to(const JPH::PhysicsSystem& physics_system, JPH::BodyID part) {
    for (const JPH::Ref<JPH::Constraint>& constraint : physics_system.GetConstraints()) {
        REQUIRE(constraint->GetType() == JPH::EConstraintType::TwoBodyConstraint);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast): Jolt has no RTTI
        const auto& joint = static_cast<const JPH::TwoBodyConstraint&>(*constraint);
        if (joint.GetBody2()->GetID() == part) {
            return joint.GetConstraintPriority();
        }
    }
    FAIL("no constraint joins the part to its parent part");
    return 0;
}

} // namespace

TEST_CASE("a character's physics pose starts as its rig's bind pose, placed by its root transform",
          "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const JPH::Quat quarter_turn = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), 0.5F * JPH::JPH_PI);

    const auto character = world->add_character(
        chain_rig(), {.position = JPH::Vec3(2.0F, 0.0F, -1.0F), .rotation = quarter_turn});

    REQUIRE(character.has_value());
    const aplomb::Pose& pose = world->character(*character).physics_pose();
    REQUIRE(pose.part_count() == 3);
    // The root part's transform is relative to the world, and every other part's to its parent
    // part's.
    CHECK(pose[pelvis].position.IsClose(JPH::Vec3(2.0F, 0.95F, -1.0F), close_enough_squared));
    CHECK(pose[pelvis].rotation.IsClose(quarter_turn, close_enough_squared));
    CHECK(pose[thigh].position.IsClose(JPH::Vec3(0.0F, -0.3F, 0.0F), close_enough_squared));
    CHECK(pose[thigh].rotation.IsClose(JPH::Quat::sIdentity(), close_enough_squared));
    CHECK(pose[shin].position.IsClose(JPH::Vec3(0.0F, -0.4F, 0.0F), close_enough_squared));
    CHECK(pose[shin].rotation.IsClose(JPH::Quat::sIdentity(), close_enough_squared));
}

TEST_CASE("removing a character takes its bodies and constraints out of the physics system",
          "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const auto kept = world->add_character(chain_rig(), {});
    const auto removed =
        world->add_character(chain_rig(), {.position = JPH::Vec3(2.0F, 0.0F, 0.0F)});
    REQUIRE(kept.has_value());
    REQUIRE(removed.has_value());
    REQUIRE(world->physics_system().GetNumBodies() == 6);
    REQUIRE(world->physics_system().GetConstraints().size() == 4);

    world->remove_character(*removed);

    CHECK(world->physics_system().GetNumBodies() == 3);
    CHECK(world->physics_system().GetConstraints().size() == 2);
    CHECK(world->physics_system().GetBodyInterface().IsAdded(
        world->character(*kept).part_body(pelvis)));
}

TEST_CASE(
    "characters are numbered in the order they are added, and a removed one's id isn't reused",
    "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());

    const auto first = world->add_character(chain_rig(), {});
    const auto second = world->add_character(chain_rig(), {});
    world->remove_character(aplomb::CharacterId(1));
    const auto third = world->add_character(chain_rig(), {});

    CHECK(first == aplomb::CharacterId(0));
    CHECK(second == aplomb::CharacterId(1));
    CHECK(third == aplomb::CharacterId(2));
}

TEST_CASE("a character's joints follow its rig's parts, the root part's excepted", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    // Parts may come before their parents.
    aplomb::RigDesc rig = chain_rig();
    std::ranges::reverse(rig.parts);

    const auto id = world->add_character(rig, {});

    REQUIRE(id.has_value());
    const aplomb::Character& character = world->character(*id);
    REQUIRE(character.joint_count() == 2);
    CHECK(character.joint_part(aplomb::JointId(0)) == aplomb::PartId(0));
    CHECK(character.joint_part(aplomb::JointId(1)) == aplomb::PartId(1));
    // The parts' ids follow the reversed order: the shin is first now, and the pelvis last.
    const aplomb::Pose& pose = character.physics_pose();
    CHECK(pose[aplomb::PartId(0)].position.IsClose(JPH::Vec3(0.0F, -0.4F, 0.0F),
                                                   close_enough_squared));
    CHECK(pose[aplomb::PartId(2)].position.IsClose(JPH::Vec3(0.0F, 0.95F, 0.0F),
                                                   close_enough_squared));
}

TEST_CASE("a character keeps its rig's muscle settings", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());

    const auto id = world->add_character(chain_rig(), {});

    REQUIRE(id.has_value());
    const aplomb::RigDesc& rig = world->character(*id).rig();
    REQUIRE(rig.parts.size() == 3);
    const aplomb::MuscleDesc& muscle = required(rig.parts[shin.index()].muscle);
    CHECK(muscle.stiffness == 200.0F);
    CHECK(muscle.damping == 20.0F);
    CHECK(muscle.max_torque == 120.0F);
}

TEST_CASE("parts joined to each other don't collide", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    // Two spheres that overlap, joined at a point off the line between their centres, so that a
    // collision between them would swing one about the other.
    const aplomb::RigDesc rig{
        .name = "overlapping",
        .parts =
            {
                {
                    .name = "first",
                    .shape = aplomb::SphereDesc{.radius = 0.2F},
                    .mass = 1.0F,
                },
                {
                    .name = "second",
                    .parent = "first",
                    .shape = aplomb::SphereDesc{.radius = 0.2F},
                    .mass = 1.0F,
                    .bind_position = JPH::Vec3(0.25F, 0.0F, 0.0F),
                    .joint =
                        aplomb::SwingTwistJointDesc{
                            .anchor = JPH::Vec3(0.125F, 0.3F, 0.0F),
                            .twist_axis = JPH::Vec3(1.0F, 0.0F, 0.0F),
                            .plane_axis = JPH::Vec3(0.0F, 1.0F, 0.0F),
                            .normal_half_cone = 1.5F,
                            .plane_half_cone = 1.5F,
                            .twist_min = -1.5F,
                            .twist_max = 1.5F,
                        },
                },
            },
    };
    const auto id = world->add_character(rig, {.position = JPH::Vec3(0.0F, 10.0F, 0.0F)});
    REQUIRE(id.has_value());

    for (int step = 0; step < 30; ++step) {
        world->step();
    }

    // Falling freely together, neither has moved relative to the other.
    const aplomb::Pose& pose = world->character(*id).physics_pose();
    CHECK(pose[aplomb::PartId(1)].position.IsClose(JPH::Vec3(0.25F, 0.0F, 0.0F), 1e-6F));
    CHECK(pose[aplomb::PartId(1)].rotation.IsClose(JPH::Quat::sIdentity(), 1e-6F));
}

TEST_CASE("a joint nearer the root part is solved with a higher priority", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    const auto id = world->add_character(chain_rig(), {});
    REQUIRE(id.has_value());
    const aplomb::Character& character = world->character(*id);

    CHECK(priority_of_joint_to(world->physics_system(), character.part_body(thigh)) >
          priority_of_joint_to(world->physics_system(), character.part_body(shin)));
}

TEST_CASE("a character dropped onto flat ground comes to rest on it", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    aplomb::build_flat_ground(world->physics_system());
    // Tilted, since a chain dropped upright would land balanced on the shin and stay so.
    const auto id = world->add_character(
        chain_rig(), {
                         .position = JPH::Vec3(0.0F, 0.5F, 0.0F),
                         .rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), 0.3F),
                     });
    REQUIRE(id.has_value());
    const aplomb::Character& character = world->character(*id);

    // Five seconds at 60 Hz.
    for (int step = 0; step < 300; ++step) {
        world->step();
    }

    // The chain has toppled, and the pelvis lies no higher than its own size and the thigh's lift
    // it, no lower than Jolt lets a body sink into the ground, and still.
    const JPH::Vec3 pelvis_position = character.physics_pose()[pelvis].position;
    CHECK(pelvis_position.GetY() > 0.08F);
    CHECK(pelvis_position.GetY() < 0.25F);
    CHECK(world->physics_system()
              .GetBodyInterface()
              .GetLinearVelocity(character.part_body(pelvis))
              .Length() < 0.01F);
}
