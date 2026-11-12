#include <aplomb/core/character_error.hpp>

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "required.hpp"
#include "rigs.hpp"
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr float not_a_number = std::numeric_limits<float>::quiet_NaN();
constexpr float infinity = std::numeric_limits<float>::infinity();

// The parts of chain_rig().
constexpr std::size_t pelvis = 0;
constexpr std::size_t thigh = 1;
constexpr std::size_t shin = 2;

aplomb::SwingTwistJointDesc& thigh_joint(aplomb::RigDesc& rig) {
    return std::get<aplomb::SwingTwistJointDesc>(required(rig.parts[thigh].joint));
}

aplomb::HingeJointDesc& shin_joint(aplomb::RigDesc& rig) {
    return std::get<aplomb::HingeJointDesc>(required(rig.parts[shin].joint));
}

// One mistake made in chain_rig(), and the one problem it must give.
struct Mistake {
    std::string_view description;
    void (*make)(aplomb::RigDesc& rig);
    aplomb::RigProblemCode code;
    std::optional<std::size_t> part;
};

// Adds a character to a world of its own.
std::expected<aplomb::CharacterId, aplomb::CharacterError>
add_character(const aplomb::RigDesc& rig) {
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    return world->add_character(rig, {});
}

} // namespace

TEST_CASE("a character isn't added from a rig with a mistake, and the problem names its part",
          "[core]") {
    using enum aplomb::RigProblemCode;
    const JoltRuntime runtime;
    const Mistake mistake = GENERATE(Catch::Generators::values<Mistake>({
        {"two parts with one name",
         [](aplomb::RigDesc& rig) { rig.parts[shin].name = "thigh"; }, duplicate_part_name, shin},
        {"a parent that no part is named",
         [](aplomb::RigDesc& rig) { rig.parts[shin].parent = "knee"; }, unknown_parent, shin},
        {"no parts", [](aplomb::RigDesc& rig) { rig.parts.clear(); }, no_root_part, std::nullopt},
        {"two parts without a parent",
         [](aplomb::RigDesc& rig) {
             rig.parts[shin].parent.reset();
             rig.parts[shin].joint.reset();
             rig.parts[shin].muscle.reset();
         },
         several_root_parts, shin},
        {"a part that is its own parent",
         [](aplomb::RigDesc& rig) { rig.parts[shin].parent = "shin"; }, cycle, shin},
        {"a joint on the root part",
         [](aplomb::RigDesc& rig) { rig.parts[pelvis].joint = rig.parts[thigh].joint; },
         joint_on_root_part, pelvis},
        {"a part other than the root part without a joint",
         [](aplomb::RigDesc& rig) { rig.parts[shin].joint.reset(); }, missing_joint, shin},
        {"a muscle on the root part",
         [](aplomb::RigDesc& rig) { rig.parts[pelvis].muscle = aplomb::MuscleDesc{}; },
         muscle_on_root_part, pelvis},
        {"a mass of 0", [](aplomb::RigDesc& rig) { rig.parts[thigh].mass = 0.0F; }, invalid_mass,
         thigh},
        {"a negative mass", [](aplomb::RigDesc& rig) { rig.parts[thigh].mass = -1.0F; },
         invalid_mass, thigh},
        {"an infinite mass", [](aplomb::RigDesc& rig) { rig.parts[thigh].mass = infinity; },
         invalid_mass, thigh},
        {"a mass that isn't a number", [](aplomb::RigDesc& rig) { rig.parts[thigh].mass = not_a_number; },
         invalid_mass, thigh},
        {"a box with a half extent of 0",
         [](aplomb::RigDesc& rig) {
             rig.parts[pelvis].shape = aplomb::BoxDesc{.half_extents = JPH::Vec3(0.1F, 0.0F, 0.1F)};
         },
         invalid_size, pelvis},
        {"a capsule with a negative radius",
         [](aplomb::RigDesc& rig) {
             std::get<aplomb::CapsuleDesc>(rig.parts[thigh].shape).radius = -0.1F;
         },
         invalid_size, thigh},
        {"a capsule with a half height of 0",
         [](aplomb::RigDesc& rig) {
             std::get<aplomb::CapsuleDesc>(rig.parts[thigh].shape).half_height = 0.0F;
         },
         invalid_size, thigh},
        {"a sphere whose radius isn't a number",
         [](aplomb::RigDesc& rig) { rig.parts[shin].shape = aplomb::SphereDesc{.radius = not_a_number}; },
         invalid_size, shin},
        {"a bind position that isn't finite",
         [](aplomb::RigDesc& rig) { rig.parts[shin].bind_position = JPH::Vec3(not_a_number, 0.0F, 0.0F); },
         non_finite_position, shin},
        {"an anchor that isn't finite",
         [](aplomb::RigDesc& rig) { shin_joint(rig).anchor = JPH::Vec3(0.0F, infinity, 0.0F); },
         non_finite_position, shin},
        {"a bind rotation that isn't a unit quaternion",
         [](aplomb::RigDesc& rig) {
             rig.parts[thigh].bind_rotation = JPH::Quat(0.0F, 0.0F, 0.0F, 2.0F);
         },
         non_unit_rotation, thigh},
        {"a bind rotation that isn't a number",
         [](aplomb::RigDesc& rig) {
             rig.parts[thigh].bind_rotation = JPH::Quat(not_a_number, 0.0F, 0.0F, 1.0F);
         },
         non_unit_rotation, thigh},
        {"a twist axis that isn't a unit vector",
         [](aplomb::RigDesc& rig) { thigh_joint(rig).twist_axis = JPH::Vec3(0.0F, -2.0F, 0.0F); },
         non_unit_axis, thigh},
        {"a hinge axis of 0",
         [](aplomb::RigDesc& rig) { shin_joint(rig).hinge_axis = JPH::Vec3::sZero(); },
         non_unit_axis, shin},
        {"a plane axis along the twist axis",
         [](aplomb::RigDesc& rig) { thigh_joint(rig).plane_axis = JPH::Vec3(0.0F, -1.0F, 0.0F); },
         axes_not_perpendicular, thigh},
        {"a normal axis that isn't perpendicular to the hinge axis",
         [](aplomb::RigDesc& rig) { shin_joint(rig).normal_axis = JPH::Vec3(0.6F, -0.8F, 0.0F); },
         axes_not_perpendicular, shin},
        {"a negative normal half cone",
         [](aplomb::RigDesc& rig) { thigh_joint(rig).normal_half_cone = -0.1F; },
         limit_out_of_range, thigh},
        {"a plane half cone above π",
         [](aplomb::RigDesc& rig) { thigh_joint(rig).plane_half_cone = 3.2F; },
         limit_out_of_range, thigh},
        {"a twist_min below -π", [](aplomb::RigDesc& rig) { thigh_joint(rig).twist_min = -3.2F; },
         limit_out_of_range, thigh},
        {"a twist_max that isn't a number",
         [](aplomb::RigDesc& rig) { thigh_joint(rig).twist_max = not_a_number; }, limit_out_of_range,
         thigh},
        {"a twist_min above the twist_max",
         [](aplomb::RigDesc& rig) {
             thigh_joint(rig).twist_min = 0.3F;
             thigh_joint(rig).twist_max = 0.2F;
         },
         limit_out_of_range, thigh},
        {"a positive limit_min", [](aplomb::RigDesc& rig) { shin_joint(rig).limit_min = 0.5F; },
         limit_out_of_range, shin},
        {"a limit_max above π", [](aplomb::RigDesc& rig) { shin_joint(rig).limit_max = 3.2F; },
         limit_out_of_range, shin},
        {"hinge limits that are both 0",
         [](aplomb::RigDesc& rig) { shin_joint(rig).limit_min = 0.0F; }, limit_out_of_range,
         shin},
        {"a negative stiffness",
         [](aplomb::RigDesc& rig) { rig.parts[thigh].muscle->stiffness = -1.0F; },
         invalid_muscle_setting, thigh},
        {"a damping that isn't a number",
         [](aplomb::RigDesc& rig) { rig.parts[thigh].muscle->damping = not_a_number; },
         invalid_muscle_setting, thigh},
        {"an infinite max_torque",
         [](aplomb::RigDesc& rig) { rig.parts[shin].muscle->max_torque = infinity; },
         invalid_muscle_setting, shin},
    }));
    INFO("a rig with " << mistake.description);
    aplomb::RigDesc rig = chain_rig();
    mistake.make(rig);

    const auto character = add_character(rig);

    REQUIRE_FALSE(character.has_value());
    const aplomb::CharacterError& error = character.error();
    CHECK(error.code == aplomb::CharacterErrorCode::invalid_rig);
    REQUIRE(error.problems.size() == 1);
    CHECK(error.problems.front().code == mistake.code);
    const std::optional<aplomb::PartId> part = mistake.part.transform(
        [](std::size_t index) { return aplomb::PartId(static_cast<std::uint32_t>(index)); });
    CHECK(error.problems.front().part == part);
}

TEST_CASE("a character's rig may be close to unit lengths and right angles, as a person writes "
          "them",
          "[core]") {
    const JoltRuntime runtime;
    aplomb::RigDesc rig = chain_rig();
    // An eighth of a turn about Z, and axes, to four decimal places.
    rig.parts[thigh].bind_rotation = JPH::Quat(0.0F, 0.0F, 0.3827F, 0.9239F);
    thigh_joint(rig).twist_axis = JPH::Vec3(0.7071F, -0.7071F, 0.0F);
    thigh_joint(rig).plane_axis = JPH::Vec3(0.7071F, 0.7071F, 0.0F);

    CHECK(add_character(rig).has_value());
}

TEST_CASE("every problem with a rig is reported at once, those with its structure first",
          "[core]") {
    using enum aplomb::RigProblemCode;
    const JoltRuntime runtime;
    aplomb::RigDesc rig = chain_rig();
    rig.parts[thigh].mass = -1.0F;
    rig.parts[pelvis].muscle = aplomb::MuscleDesc{};
    rig.parts[shin].parent = "knee";

    const auto character = add_character(rig);

    REQUIRE_FALSE(character.has_value());
    std::vector<aplomb::RigProblemCode> codes;
    for (const aplomb::RigProblem& problem : character.error().problems) {
        codes.push_back(problem.code);
    }
    CHECK(codes == std::vector{unknown_parent, muscle_on_root_part, invalid_mass});
}

TEST_CASE("a character error formats as its message, which names the rig and each problem",
          "[core]") {
    const JoltRuntime runtime;
    aplomb::RigDesc rig = chain_rig();
    rig.parts[thigh].mass = -1.0F;
    rig.parts[shin].parent = "knee";

    const auto character = add_character(rig);

    REQUIRE_FALSE(character.has_value());
    const aplomb::CharacterError& error = character.error();
    REQUIRE(error.problems.size() == 2);
    CHECK(std::format("{}", error.problems[0]) ==
          "part \"shin\" has the parent \"knee\", but no part has that name");
    CHECK(std::format("{}", error.problems[1]) ==
          "the mass of part \"thigh\" must be a positive, finite number of kilograms, not -1");
    CHECK(std::format("{}", error) ==
          "a character can't be made from the rig \"chain\": part \"shin\" has the parent "
          "\"knee\", but no part has that name; the mass of part \"thigh\" must be a positive, "
          "finite number of kilograms, not -1");
}

TEST_CASE("a character isn't added when the physics system has no room for its bodies", "[core]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    // Fill the physics system with characters until one doesn't fit.
    JPH::uint bodies_before = world->physics_system().GetNumBodies();
    auto character = world->add_character(chain_rig(), {});
    const JPH::uint max_bodies = world->physics_system().GetMaxBodies();
    for (JPH::uint attempt = 0; attempt < max_bodies && character.has_value(); ++attempt) {
        bodies_before = world->physics_system().GetNumBodies();
        character = world->add_character(chain_rig(), {});
    }

    REQUIRE_FALSE(character.has_value());
    CHECK(character.error().code == aplomb::CharacterErrorCode::out_of_bodies);
    CHECK(character.error().problems.empty());
    // A character that doesn't fit leaves nothing of itself behind.
    CHECK(world->physics_system().GetNumBodies() == bodies_before);
}
