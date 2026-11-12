#include <aplomb/core/rig_problem.hpp>

#include "required.hpp"
#include "rigs.hpp"
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <vector>

namespace {

// The parts of chain_rig().
const aplomb::PartId thigh(1);
const aplomb::PartId shin(2);

} // namespace

TEST_CASE("a rig with no mistakes has no problems", "[core]") {
    CHECK(aplomb::rig_problems(chain_rig()).empty());
}

TEST_CASE("a rig's problems are found without a world, each with its part and field", "[core]") {
    using enum aplomb::RigProblemCode;
    aplomb::RigDesc rig = chain_rig();
    rig.parts[thigh.index()].mass = -1.0F;
    rig.parts[shin.index()].parent = "knee";

    const std::vector<aplomb::RigProblem> problems = aplomb::rig_problems(rig);

    REQUIRE(problems.size() == 2);
    CHECK(problems[0].code == unknown_parent);
    CHECK(problems[0].part == shin);
    CHECK(problems[0].field == "parent");
    CHECK(problems[1].code == invalid_mass);
    CHECK(problems[1].part == thigh);
    CHECK(problems[1].field == "mass");
}

TEST_CASE("a joint's limit out of its range is given in degrees as well as radians", "[core]") {
    aplomb::RigDesc rig = chain_rig();
    auto& joint = std::get<aplomb::SwingTwistJointDesc>(required(rig.parts[thigh.index()].joint));
    // 200°.
    joint.twist_max = 3.4906585F;

    const std::vector<aplomb::RigProblem> problems = aplomb::rig_problems(rig);

    REQUIRE(problems.size() == 1);
    CHECK(std::format("{}", problems[0]) ==
          "the twist_max of part \"thigh\"'s joint must be from -π to π radians, -180° to 180°, "
          "not 3.4906585 radians, 200°");
}

TEST_CASE("a twist_min above the twist_max is given in degrees as well as radians", "[core]") {
    aplomb::RigDesc rig = chain_rig();
    auto& joint = std::get<aplomb::SwingTwistJointDesc>(required(rig.parts[thigh.index()].joint));
    joint.twist_min = 0.5F;
    joint.twist_max = 0.25F;

    const std::vector<aplomb::RigProblem> problems = aplomb::rig_problems(rig);

    REQUIRE(problems.size() == 1);
    CHECK(std::format("{}", problems[0]) ==
          "the twist_min of part \"thigh\"'s joint, 0.5 radians or 28.6479°, must be no greater "
          "than its twist_max, 0.25 radians or 14.3239°");
}
