#include <aplomb/files/rig_file.hpp>

#include "required.hpp"
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float close_enough = 1e-6F;

// A rig file of three parts in a chain: a box pelvis, a capsule thigh on a swing-twist joint with a
// muscle, and a sphere shin on a hinge. The line numbers below count from the opening brace.
constexpr std::string_view chain_file = R"({
  "format": "aplomb.rig",
  "version": 1,
  "name": "chain",
  "parts": [
    {
      "name": "pelvis",
      "parent": null,
      "shape": {"type": "box", "half_extents": [0.15, 0.1, 0.1]},
      "mass": 10,
      "bind_position": [0, 0.95, 0],
      "bind_rotation": [0, 0, 0, 1]
    },
    {
      "name": "thigh",
      "parent": "pelvis",
      "shape": {"type": "capsule", "radius": 0.07, "half_height": 0.15},
      "mass": 8,
      "bind_position": [0, 0.65, 0],
      "bind_rotation": [0, 0, 0, 1],
      "friction": 0.5,
      "joint": {
        "type": "swing_twist",
        "anchor": [0, 0.85, 0],
        "twist_axis": [0, -1, 0],
        "plane_axis": [1, 0, 0],
        "normal_half_cone": 60,
        "plane_half_cone": 45,
        "twist_min": -20,
        "twist_max": 20
      },
      "muscle": {"stiffness": 400, "damping": 40, "max_torque": 250}
    },
    {
      "name": "shin",
      "parent": "thigh",
      "shape": {"type": "sphere", "radius": 0.06},
      "mass": 4,
      "bind_position": [0, 0.25, 0],
      "bind_rotation": [0, 0, 0.6, 0.8],
      "joint": {
        "type": "hinge",
        "anchor": [0, 0.45, 0],
        "hinge_axis": [1, 0, 0],
        "normal_axis": [0, -1, 0],
        "limit_min": -135,
        "limit_max": 0
      }
    }
  ]
}
)";

// The shin's joint in the chain file, and the bind rotation before it.
constexpr std::string_view shin_joint_in_chain_file = R"("bind_rotation": [0, 0, 0.6, 0.8],
      "joint": {
        "type": "hinge",
        "anchor": [0, 0.45, 0],
        "hinge_axis": [1, 0, 0],
        "normal_axis": [0, -1, 0],
        "limit_min": -135,
        "limit_max": 0
      })";

bool is_close(float actual, float expected) {
    return actual >= expected - close_enough && actual <= expected + close_enough;
}

// The chain file with some of its text replaced, each `from` a text it has once.
std::string
chain_file_with(std::initializer_list<std::pair<std::string_view, std::string_view>> replacements) {
    std::string text(chain_file);
    for (const auto& [from, to] : replacements) {
        const std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        REQUIRE(text.find(from, at + 1) == std::string::npos);
        text.replace(at, from.size(), to);
    }
    return text;
}

// One mistake made in the chain file, by replacing `from` with `to`, and the one problem it must
// give.
struct Mistake {
    std::string_view description;
    std::string_view from;
    std::string_view to;
    aplomb::FileProblemCode code;
    std::size_t line;
    std::size_t column;
    std::string_view pointer;
    std::optional<aplomb::RigProblemCode> rig_problem = std::nullopt;
};

// Where a problem is, as `line:column pointer`, so that a failed check shows all of it.
std::string place_of(const aplomb::FileProblem& problem) {
    return std::format("{}:{} {}", problem.line, problem.column, problem.pointer);
}

std::vector<aplomb::FileProblemCode> codes_of(const std::vector<aplomb::FileProblem>& problems) {
    std::vector<aplomb::FileProblemCode> codes;
    codes.reserve(problems.size());
    for (const aplomb::FileProblem& problem : problems) {
        codes.push_back(problem.code);
    }
    return codes;
}

void check_one_problem(const Mistake& mistake) {
    INFO("a rig file with " << mistake.description);
    const auto rig =
        aplomb::parse_rig_file(chain_file_with({{mistake.from, mistake.to}}), "chain.json");

    REQUIRE_FALSE(rig.has_value());
    REQUIRE(rig.error().problems.size() == 1);
    const aplomb::FileProblem& problem = rig.error().problems.front();
    INFO(std::format("{}", problem));
    CHECK(std::pair(problem.code, problem.rig_problem) ==
          std::pair(mistake.code, mistake.rig_problem));
    CHECK(std::format("{} {}", problem.path, place_of(problem)) ==
          std::format("chain.json {}:{} {}", mistake.line, mistake.column, mistake.pointer));
}

} // namespace

TEST_CASE("a rig file is read into the rig it describes, its angles turned into radians",
          "[files]") {
    const auto rig = aplomb::parse_rig_file(std::string(chain_file), "chain.json");

    REQUIRE(rig.has_value());
    CHECK(rig->name == "chain");
    REQUIRE(rig->parts.size() == 3);

    const aplomb::PartDesc& pelvis = rig->parts[0];
    CHECK(pelvis.name == "pelvis");
    CHECK_FALSE(pelvis.parent.has_value());
    CHECK(std::get<aplomb::BoxDesc>(pelvis.shape).half_extents == JPH::Vec3(0.15F, 0.1F, 0.1F));
    CHECK(pelvis.mass == 10.0F);
    CHECK(pelvis.bind_position == JPH::Vec3(0.0F, 0.95F, 0.0F));
    CHECK(pelvis.bind_rotation == JPH::Quat::sIdentity());
    CHECK(pelvis.friction == aplomb::PartDesc{}.friction);
    CHECK_FALSE(pelvis.joint.has_value());
    CHECK_FALSE(pelvis.muscle.has_value());

    const aplomb::PartDesc& thigh = rig->parts[1];
    CHECK(thigh.name == "thigh");
    CHECK(thigh.parent == "pelvis");
    const auto& capsule = std::get<aplomb::CapsuleDesc>(thigh.shape);
    CHECK(capsule.radius == 0.07F);
    CHECK(capsule.half_height == 0.15F);
    CHECK(thigh.mass == 8.0F);
    CHECK(thigh.friction == 0.5F);
    const auto& swing_twist = std::get<aplomb::SwingTwistJointDesc>(required(thigh.joint));
    CHECK(swing_twist.anchor == JPH::Vec3(0.0F, 0.85F, 0.0F));
    CHECK(swing_twist.twist_axis == JPH::Vec3(0.0F, -1.0F, 0.0F));
    CHECK(swing_twist.plane_axis == JPH::Vec3(1.0F, 0.0F, 0.0F));
    CHECK(is_close(swing_twist.normal_half_cone, JPH::JPH_PI / 3.0F));
    CHECK(is_close(swing_twist.plane_half_cone, JPH::JPH_PI / 4.0F));
    CHECK(is_close(swing_twist.twist_min, -JPH::JPH_PI / 9.0F));
    CHECK(is_close(swing_twist.twist_max, JPH::JPH_PI / 9.0F));
    const aplomb::MuscleDesc& muscle = required(thigh.muscle);
    CHECK(muscle.stiffness == 400.0F);
    CHECK(muscle.damping == 40.0F);
    CHECK(muscle.max_torque == 250.0F);

    const aplomb::PartDesc& shin = rig->parts[2];
    CHECK(shin.parent == "thigh");
    CHECK(std::get<aplomb::SphereDesc>(shin.shape).radius == 0.06F);
    CHECK(shin.bind_rotation == JPH::Quat(0.0F, 0.0F, 0.6F, 0.8F));
    const auto& hinge = std::get<aplomb::HingeJointDesc>(required(shin.joint));
    CHECK(hinge.anchor == JPH::Vec3(0.0F, 0.45F, 0.0F));
    CHECK(hinge.hinge_axis == JPH::Vec3(1.0F, 0.0F, 0.0F));
    CHECK(hinge.normal_axis == JPH::Vec3(0.0F, -1.0F, 0.0F));
    CHECK(is_close(hinge.limit_min, -0.75F * JPH::JPH_PI));
    CHECK(hinge.limit_max == 0.0F);
    CHECK_FALSE(shin.muscle.has_value());
}

TEST_CASE("each mistake in a rig file's syntax or structure is one problem, with its line, column "
          "and JSON pointer",
          "[files]") {
    using enum aplomb::FileProblemCode;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"a stray comma", R"("mass": 8,)", R"("mass": 8,,)", syntax_error, 18, 17, ""},
        {"a key given twice", R"("mass": 8,)", R"("mass": 8, "mass": 9,)", duplicate_key, 18, 18,
         "/parts/1/mass"},
        {"a misspelt key", R"("friction": 0.5,)", R"("frictoin": 0.5,)", unknown_key, 21, 7,
         "/parts/1/frictoin"},
        {"a key the rig file doesn't have", R"("name": "chain",)",
         R"("name": "chain", "units": "metres",)", unknown_key, 4, 20, "/units"},
        {"a key the shape's type doesn't have", R"("radius": 0.06})",
         R"("radius": 0.06, "half_height": 0.1})", unknown_key, 37, 51,
         "/parts/2/shape/half_height"},
        {"a part without a mass", "      \"mass\": 4,\n", "", missing_key, 34, 5, "/parts/2/mass"},
        {"a muscle without a max_torque", R"("damping": 40, "max_torque": 250})",
         R"("damping": 40})", missing_key, 32, 17, "/parts/1/muscle/max_torque"},
        {"no name", "  \"name\": \"chain\",\n", "", missing_key, 1, 1, "/name"},
        {"no format", "  \"format\": \"aplomb.rig\",\n", "", missing_key, 1, 1, "/format"},
        {"no version", "  \"version\": 1,\n", "", missing_key, 1, 1, "/version"},
        {"a mass that is a string", R"("mass": 8,)", R"("mass": "8",)", wrong_type, 18, 15,
         "/parts/1/mass"},
        {"a parent that is a number", R"("parent": "thigh")", R"("parent": 1)", wrong_type, 36, 17,
         "/parts/2/parent"},
        {"an anchor with null in it", "[0, 0.85, 0]", "[0, null, 0]", wrong_type, 24, 23,
         "/parts/1/joint/anchor/1"},
        {"a version that is a string", R"("version": 1)", R"("version": "1")", wrong_type, 3, 14,
         "/version"},
        {"a bind position of two numbers", "[0, 0.65, 0]", "[0, 0.65]", wrong_length, 19, 24,
         "/parts/1/bind_position"},
        {"a mass too large for a float", R"("mass": 10,)", R"("mass": 1e39,)", number_out_of_range,
         10, 15, "/parts/0/mass"},
        {"a shape type that doesn't exist", R"("type": "sphere")", R"("type": "cone")",
         invalid_value, 37, 25, "/parts/2/shape/type"},
        {"a joint type that doesn't exist", R"("type": "hinge")", R"("type": "ball")",
         invalid_value, 42, 17, "/parts/2/joint/type"},
        {"another format", R"("aplomb.rig")", R"("aplomb.scenario")", invalid_value, 2, 13,
         "/format"},
        {"a version of 0", R"("version": 1)", R"("version": 0)", invalid_value, 3, 14, "/version"},
        {"a version with a fraction", R"("version": 1)", R"("version": 1.5)", invalid_value, 3, 14,
         "/version"},
        {"a negative version", R"("version": 1)", R"("version": -1)", invalid_value, 3, 14,
         "/version"},
        {"a newer version", R"("version": 1)", R"("version": 2)", unsupported_version, 3, 14,
         "/version"},
        {"a version too large for any integer", R"("version": 1)",
         R"("version": 100000000000000000000)", unsupported_version, 3, 14, "/version"},
    })));
}

TEST_CASE("each mistake in what a rig file describes is one problem, at the value it is with",
          "[files]") {
    using enum aplomb::RigProblemCode;
    constexpr auto invalid_rig = aplomb::FileProblemCode::invalid_rig;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"two parts with one name", R"("name": "shin")", R"("name": "thigh")", invalid_rig, 35, 15,
         "/parts/2/name", duplicate_part_name},
        {"a parent that no part is named", R"("parent": "thigh")", R"("parent": "knee")",
         invalid_rig, 36, 17, "/parts/2/parent", unknown_parent},
        {"two parts that are each other's parents", R"("parent": "pelvis")", R"("parent": "shin")",
         invalid_rig, 16, 17, "/parts/1/parent", cycle},
        {"a part other than the root part without a joint", shin_joint_in_chain_file,
         R"("bind_rotation": [0, 0, 0.6, 0.8])", invalid_rig, 34, 5, "/parts/2", missing_joint},
        {"a muscle on the root part", R"("bind_position": [0, 0.95, 0],)",
         R"("bind_position": [0, 0.95, 0], "muscle": {"stiffness": 1, "damping": 1, )"
         R"("max_torque": 1},)",
         invalid_rig, 11, 48, "/parts/0/muscle", muscle_on_root_part},
        {"a mass of 0", R"("mass": 4,)", R"("mass": 0,)", invalid_rig, 38, 15, "/parts/2/mass",
         invalid_mass},
        {"a box with a half extent of 0", "[0.15, 0.1, 0.1]", "[0.15, 0, 0.1]", invalid_rig, 9, 48,
         "/parts/0/shape/half_extents", invalid_size},
        {"a hinge axis that isn't a unit vector", R"("hinge_axis": [1, 0, 0])",
         R"("hinge_axis": [2, 0, 0])", invalid_rig, 44, 23, "/parts/2/joint/hinge_axis",
         non_unit_axis},
        {"hinge limits that are both 0", R"("limit_min": -135)", R"("limit_min": 0)", invalid_rig,
         46, 22, "/parts/2/joint/limit_min", limit_out_of_range},
        {"a negative friction", R"("friction": 0.5)", R"("friction": -0.5)", invalid_rig, 21, 19,
         "/parts/1/friction", invalid_friction},
        {"a negative radius", R"("radius": 0.06)", R"("radius": -0.06)", invalid_rig, 37, 45,
         "/parts/2/shape/radius", invalid_size},
        {"a bind rotation that isn't a unit quaternion", "[0, 0, 0.6, 0.8]", "[0, 0, 0.6, 0.6]",
         invalid_rig, 40, 24, "/parts/2/bind_rotation", non_unit_rotation},
        {"axes that aren't perpendicular", R"("plane_axis": [1, 0, 0])",
         R"("plane_axis": [0, 1, 0])", invalid_rig, 26, 23, "/parts/1/joint/plane_axis",
         axes_not_perpendicular},
        {"a twist_max of 200°", R"("twist_max": 20)", R"("twist_max": 200)", invalid_rig, 30, 22,
         "/parts/1/joint/twist_max", limit_out_of_range},
        {"a negative damping", R"("damping": 40)", R"("damping": -40)", invalid_rig, 32, 47,
         "/parts/1/muscle/damping", invalid_muscle_setting},
    })));
}

TEST_CASE("a rig problem with a part as a whole is at the part, and one with the rig at its parts",
          "[files]") {
    // The shin made a second root part, which also puts a joint on a root part.
    const auto two_roots = aplomb::parse_rig_file(
        chain_file_with({{R"("parent": "thigh")", R"("parent": null)"}}), "chain.json");
    // No part without a parent.
    const auto no_root = aplomb::parse_rig_file(
        chain_file_with({{R"("parent": null)", R"("parent": "shin")"}}), "chain.json");

    REQUIRE_FALSE(two_roots.has_value());
    std::vector<std::string> places;
    for (const aplomb::FileProblem& problem : two_roots.error().problems) {
        places.push_back(std::format("{} {}", place_of(problem),
                                     static_cast<int>(problem.rig_problem.value_or({}))));
    }
    CHECK(places == std::vector<std::string>{
                        std::format("36:17 /parts/2/parent {}",
                                    static_cast<int>(aplomb::RigProblemCode::several_root_parts)),
                        std::format("41:16 /parts/2/joint {}",
                                    static_cast<int>(aplomb::RigProblemCode::joint_on_root_part)),
                    });
    REQUIRE_FALSE(no_root.has_value());
    REQUIRE_FALSE(no_root.error().problems.empty());
    const aplomb::FileProblem& first = no_root.error().problems.front();
    CHECK(first.rig_problem == aplomb::RigProblemCode::no_root_part);
    CHECK(place_of(first) == "5:12 /parts");
}

TEST_CASE("every problem in a rig file's structure is reported at once, in the order of the file, "
          "and none in its meaning",
          "[files]") {
    using enum aplomb::FileProblemCode;
    const auto rig = aplomb::parse_rig_file(chain_file_with({
                                                {R"("twist_max": 20)", R"("twist_max": 200)"},
                                                {R"("friction": 0.5,)", R"("frictoin": 0.5,)"},
                                                {R"("mass": 10,)", R"("mass": "10",)"},
                                            }),
                                            "chain.json");

    REQUIRE_FALSE(rig.has_value());
    CHECK(rig.error().code == aplomb::FileErrorCode::invalid);
    const std::vector<aplomb::FileProblem>& problems = rig.error().problems;
    CHECK(codes_of(problems) == std::vector{wrong_type, unknown_key});
    REQUIRE(problems.size() == 2);
    CHECK(place_of(problems[0]) == "10:15 /parts/0/mass");
    CHECK(place_of(problems[1]) == "21:7 /parts/1/frictoin");
}

TEST_CASE("every problem in what a rig file describes is reported at once, in the order of the "
          "file",
          "[files]") {
    // The rig's own checks find the unknown parent first, since it is a problem with how the parts
    // hang together.
    const auto rig = aplomb::parse_rig_file(chain_file_with({
                                                {R"("parent": "thigh")", R"("parent": "knee")"},
                                                {R"("mass": 10,)", R"("mass": 0,)"},
                                                {R"("damping": 40)", R"("damping": -40)"},
                                                {R"("twist_max": 20)", R"("twist_max": 200)"},
                                            }),
                                            "chain.json");

    REQUIRE_FALSE(rig.has_value());
    std::vector<std::string> places;
    for (const aplomb::FileProblem& problem : rig.error().problems) {
        places.push_back(place_of(problem));
    }
    CHECK(places == std::vector<std::string>{
                        "10:15 /parts/0/mass",
                        "30:22 /parts/1/joint/twist_max",
                        "32:47 /parts/1/muscle/damping",
                        "36:17 /parts/2/parent",
                    });
}

TEST_CASE("a rig file that isn't JSON has only its syntax error reported", "[files]") {
    const auto rig = aplomb::parse_rig_file(chain_file_with({
                                                {R"("friction": 0.5,)", R"("frictoin": 0.5,)"},
                                                {R"("mass": 8,)", R"("mass": 8,,)"},
                                            }),
                                            "chain.json");

    REQUIRE_FALSE(rig.has_value());
    CHECK(codes_of(rig.error().problems) == std::vector{aplomb::FileProblemCode::syntax_error});
}

TEST_CASE("a rig file of another format or of a newer version is read no further", "[files]") {
    using enum aplomb::FileProblemCode;
    const std::string_view header = GENERATE(R"("aplomb.scenario")", R"("version": 2)");
    const auto rig = aplomb::parse_rig_file(
        chain_file_with({
            {header.starts_with("\"version") ? R"("version": 1)" : R"("aplomb.rig")", header},
            {R"("friction": 0.5,)", R"("frictoin": 0.5,)"},
        }),
        "chain.json");

    REQUIRE_FALSE(rig.has_value());
    REQUIRE(rig.error().problems.size() == 1);
    CHECK(rig.error().problems.front().pointer != "/parts/1/frictoin");
}

TEST_CASE("a rig file whose root isn't an object has that one problem", "[files]") {
    const auto rig = aplomb::parse_rig_file("[]", "list.json");

    REQUIRE_FALSE(rig.has_value());
    REQUIRE(rig.error().problems.size() == 1);
    CHECK(std::format("{}", rig.error().problems.front()) ==
          "list.json:1:1: expected an object, not an array");
}

TEST_CASE("a rig file's problems read as a compiler's diagnostics, and its error names them all",
          "[files]") {
    const auto rig = aplomb::parse_rig_file(chain_file_with({
                                                {R"("version": 1)", R"("version": 3)"},
                                            }),
                                            "rigs/chain.json");
    const auto meaning = aplomb::parse_rig_file(chain_file_with({
                                                    {R"("mass": 4,)", R"("mass": 0,)"},
                                                    {R"("twist_max": 20)", R"("twist_max": 200)"},
                                                }),
                                                "chain.json");

    REQUIRE_FALSE(rig.has_value());
    REQUIRE(rig.error().problems.size() == 1);
    CHECK(std::format("{}", rig.error().problems.front()) ==
          "rigs/chain.json:3:14: /version: the rig file is of version 3, newer than version 1, "
          "the newest this reader knows");
    REQUIRE_FALSE(meaning.has_value());
    CHECK(std::format("{}", meaning.error()) ==
          "the rig file \"chain.json\" has 2 problems: chain.json:30:22: "
          "/parts/1/joint/twist_max: the twist_max of part \"thigh\"'s joint must be from -π to "
          "π radians, -180° to 180°, not 3.4906585 radians, 200°; chain.json:38:15: "
          "/parts/2/mass: the mass of part \"shin\" must be a positive, finite number of "
          "kilograms, not 0");
}

TEST_CASE("a rig file that doesn't exist can't be read, and the error names it", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_rig_file.json";

    const auto rig = aplomb::load_rig_file(path);

    REQUIRE_FALSE(rig.has_value());
    CHECK(rig.error().code == aplomb::FileErrorCode::unreadable);
    CHECK(rig.error().problems.empty());
    CHECK(std::format("{}", rig.error()) ==
          std::format("the rig file \"{}\" can't be read: there is no such file", path.string()));
}

TEST_CASE("a directory can't be read as a rig file", "[files]") {
    const std::filesystem::path path = std::filesystem::temp_directory_path();

    const auto rig = aplomb::load_rig_file(path);

    REQUIRE_FALSE(rig.has_value());
    CHECK(rig.error().code == aplomb::FileErrorCode::unreadable);
    CHECK(std::format("{}", rig.error()) ==
          std::format("the rig file \"{}\" can't be read: it isn't a regular file", path.string()));
}

TEST_CASE("a rig file's hash is the 64-bit FNV-1a of its text", "[files]") {
    CHECK(aplomb::rig_file_hash("") == 0xcbf29ce484222325);
    CHECK(aplomb::rig_file_hash("a") == 0xaf63dc4c8601ec8c);
    CHECK(aplomb::rig_file_hash("foobar") == 0x85944171f73967e8);
}

TEST_CASE("a rig file's hash is read from its path as its text gives it", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_hashed.json";
    {
        std::ofstream file(path, std::ios::binary);
        file << "foobar";
    }

    const auto hash = aplomb::load_rig_file_hash(path);

    std::filesystem::remove(path);
    REQUIRE(hash.has_value());
    CHECK(*hash == 0x85944171f73967e8);
}

TEST_CASE("the hash of a rig file that doesn't exist can't be read, and the error names it",
          "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_rig_file.json";

    const auto hash = aplomb::load_rig_file_hash(path);

    REQUIRE_FALSE(hash.has_value());
    CHECK(hash.error().code == aplomb::FileErrorCode::unreadable);
    CHECK(std::format("{}", hash.error()) ==
          std::format("the rig file \"{}\" can't be read: there is no such file", path.string()));
}
