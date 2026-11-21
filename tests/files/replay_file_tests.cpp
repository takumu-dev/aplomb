#include <aplomb/files/replay_file.hpp>

#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// A replay of a character dropped onto flat ground and pushed in its first step.
aplomb::ReplayDesc pushed_replay() {
    return {
        .aplomb_version = "0.1.0",
        .jolt_version = "5.6.0",
        .rig_file_hash = 0x0123456789abcdef,
        .step_rate = 60.0F,
        .seed = 7,
        .scene = "flat_ground",
        .initial =
            {
                .characters = {{.root_position = JPH::Vec3(0.0F, 0.2F, 0.0F)}},
                .state_hash = 0x00000000000000ff,
            },
        .steps =
            {
                {
                    .pushes =
                        {
                            {
                                .character = aplomb::CharacterId(0),
                                .part = aplomb::PartId(2),
                                .world_point = JPH::Vec3(0.1F, 1.25F, -0.5F),
                                .impulse = JPH::Vec3(0.0F, 0.0F, -40.0F),
                            },
                        },
                    .state_hash = 0xfedcba9876543210,
                },
                {.state_hash = 0x1111111111111111},
            },
    };
}

// A float's bits, so that a check tells negative zero from zero.
std::string bits(float number) {
    return std::format("{:08x}", std::bit_cast<std::uint32_t>(number));
}

std::string bits(JPH::Vec3 vector) {
    return std::format("[{} {} {}]", bits(vector.GetX()), bits(vector.GetY()), bits(vector.GetZ()));
}

// Each thing a replay holds, one to a line and every float by its bits, so that a failed check
// shows all of it.
std::vector<std::string> describe(const aplomb::ReplayDesc& replay) {
    std::vector<std::string> lines{
        std::format("aplomb {}", replay.aplomb_version),
        std::format("jolt {}", replay.jolt_version),
        std::format("rig file hash {:016x}", replay.rig_file_hash),
        std::format("step rate {}", bits(replay.step_rate)),
        std::format("seed {}", replay.seed),
        std::format("scene {}", replay.scene),
        std::format("initial state hash {:016x}", replay.initial.state_hash),
    };
    for (const aplomb::InitialState& character : replay.initial.characters) {
        lines.push_back(
            std::format("character at {} in {} pose", bits(character.root_position),
                        character.pose == aplomb::InitialPose::bind ? "bind" : "another"));
    }
    for (const aplomb::RecordedStep& step : replay.steps) {
        for (const aplomb::Push& push : step.pushes) {
            lines.push_back(std::format("push {} {} at {} by {}", push.character.index(),
                                        push.part.index(), bits(push.world_point),
                                        bits(push.impulse)));
        }
        lines.push_back(std::format("step to {:016x}", step.state_hash));
    }
    return lines;
}

// A replay file of one character pushed in its first step, laid out compactly, as a person might
// write one. The line numbers below count from the opening brace.
constexpr std::string_view pushed_file = R"({
  "format": "aplomb.replay",
  "version": 1,
  "aplomb_version": "0.1.0",
  "jolt_version": "5.6.0",
  "rig_file_hash": "0123456789abcdef",
  "step_rate": 60,
  "seed": 7,
  "scene": "flat_ground",
  "initial": {"characters": [{"root_position": [0, 0.2, 0], "pose": "bind"}], "state_hash": "00000000000000ff"},
  "steps": [
    {"messages": [{"message": "push", "character": 0, "part": 2, "world_point": [0.1, 1.25, -0.5], "impulse": [0, 0, -40]}], "state_hash": "fedcba9876543210"},
    {"messages": [], "state_hash": "1111111111111111"}
  ]
}
)";

// One mistake made in the pushed file, by replacing `from`, a text it has once, with `to`, and the
// one problem it must give.
struct Mistake {
    std::string_view description;
    std::string_view from;
    std::string_view to;
    aplomb::FileProblemCode code;
    std::size_t line;
    std::size_t column;
    std::string_view pointer;
};

// The pushed file with `from`, a text it has once, replaced with `to`.
std::string pushed_file_with(std::string_view from, std::string_view to) {
    std::string text(pushed_file);
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    REQUIRE(text.find(from, at + 1) == std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

void check_one_problem(const Mistake& mistake) {
    INFO("a replay file with " << mistake.description);
    const auto replay =
        aplomb::parse_replay_file(pushed_file_with(mistake.from, mistake.to), "pushed.json");

    REQUIRE_FALSE(replay.has_value());
    REQUIRE(replay.error().code == aplomb::FileErrorCode::invalid);
    REQUIRE(replay.error().problems.size() == 1);
    const aplomb::FileProblem& problem = replay.error().problems.front();
    INFO(std::format("{}", problem));
    CHECK(problem.code == mistake.code);
    CHECK(std::format("{} {}:{} {}", problem.path, problem.line, problem.column, problem.pointer) ==
          std::format("pushed.json {}:{} {}", mistake.line, mistake.column, mistake.pointer));
}

} // namespace

TEST_CASE("a replay file gives its header, its initial state and each step's messages and state "
          "hash",
          "[files]") {
    CHECK(aplomb::write_replay_file(pushed_replay()) == R"({
  "format": "aplomb.replay",
  "version": 1,
  "aplomb_version": "0.1.0",
  "jolt_version": "5.6.0",
  "rig_file_hash": "0123456789abcdef",
  "step_rate": 60,
  "seed": 7,
  "scene": "flat_ground",
  "initial": {
    "characters": [
      {
        "root_position": [
          0,
          0.2,
          0
        ],
        "pose": "bind"
      }
    ],
    "state_hash": "00000000000000ff"
  },
  "steps": [
    {
      "messages": [
        {
          "message": "push",
          "character": 0,
          "part": 2,
          "world_point": [
            0.1,
            1.25,
            -0.5
          ],
          "impulse": [
            0,
            0,
            -40
          ]
        }
      ],
      "state_hash": "fedcba9876543210"
    },
    {
      "messages": [],
      "state_hash": "1111111111111111"
    }
  ]
}
)");
}

TEST_CASE("a replay file reads back as the replay it was written from, every float with the same "
          "bits",
          "[files]") {
    aplomb::ReplayDesc replay = pushed_replay();
    replay.step_rate = 59.9F;
    replay.seed = std::numeric_limits<std::uint64_t>::max();
    replay.initial.characters.push_back({
        .root_position = JPH::Vec3(-0.0F, std::numeric_limits<float>::denorm_min(),
                                   std::numeric_limits<float>::max()),
    });
    replay.steps.push_back({
        .pushes =
            {
                {
                    .character = aplomb::CharacterId(1),
                    .part = aplomb::PartId(15),
                    .world_point =
                        JPH::Vec3(1.0F / 3.0F, -std::numeric_limits<float>::min(), 0.1F + 0.2F),
                    .impulse = JPH::Vec3(-0.0F, 1e-30F, 16777217.0F),
                },
                {
                    .character = aplomb::CharacterId(0),
                    .part = aplomb::PartId(0),
                    .world_point = JPH::Vec3(std::numeric_limits<float>::lowest(), 0.0F, 1e38F),
                    .impulse = JPH::Vec3(2.5F, -2.5F, 7e-45F),
                },
            },
        .state_hash = 0,
    });

    const auto read = aplomb::parse_replay_file(aplomb::write_replay_file(replay), "replay.json");

    INFO((read.has_value() ? std::string() : std::format("{}", read.error())));
    REQUIRE(read.has_value());
    CHECK(describe(*read) == describe(replay));
}

TEST_CASE("a replay file is read into the replay it describes", "[files]") {
    const auto replay = aplomb::parse_replay_file(std::string(pushed_file), "pushed.json");

    INFO((replay.has_value() ? std::string() : std::format("{}", replay.error())));
    REQUIRE(replay.has_value());
    CHECK(describe(*replay) == describe(pushed_replay()));
}

TEST_CASE("each mistake in a replay file's syntax or structure is one problem, with its line, "
          "column and JSON pointer",
          "[files]") {
    using enum aplomb::FileProblemCode;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"a stray comma", R"("seed": 7,)", R"("seed": 7,,)", syntax_error, 8, 13, ""},
        {"a key given twice", R"("seed": 7,)", R"("seed": 7, "seed": 8,)", duplicate_key, 8, 14,
         "/seed"},
        {"a key the replay doesn't have", R"("seed": 7,)", R"("seed": 7, "name": "drop",)",
         unknown_key, 8, 14, "/name"},
        {"no scene", "  \"scene\": \"flat_ground\",\n", "", missing_key, 1, 1, "/scene"},
        {"a step rate that is a string", R"("step_rate": 60)", R"("step_rate": "60")", wrong_type,
         7, 16, "/step_rate"},
        {"a rig file hash of 15 digits", R"("0123456789abcdef")", R"("0123456789abcde")",
         invalid_value, 6, 20, "/rig_file_hash"},
        {"a rig file hash that isn't hexadecimal", R"("0123456789abcdef")", R"("0123456789abcdeg")",
         invalid_value, 6, 20, "/rig_file_hash"},
        {"a rig file hash in upper case", R"("0123456789abcdef")", R"("0123456789ABCDEF")",
         invalid_value, 6, 20, "/rig_file_hash"},
        {"a state hash that is a number", R"("state_hash": "1111111111111111")",
         R"("state_hash": 1)", wrong_type, 13, 36, "/steps/1/state_hash"},
        {"a negative seed", R"("seed": 7)", R"("seed": -7)", invalid_value, 8, 11, "/seed"},
        {"a character with a fraction", R"("character": 0)", R"("character": 0.5)", invalid_value,
         12, 52, "/steps/0/messages/0/character"},
        {"a part that is a name", R"("part": 2)", R"("part": "chest")", wrong_type, 12, 63,
         "/steps/0/messages/0/part"},
        {"a message that doesn't exist", R"("message": "push")", R"("message": "hit")",
         invalid_value, 12, 31, "/steps/0/messages/0/message"},
        {"a push without an impulse", R"(, "impulse": [0, 0, -40])", "", missing_key, 12, 19,
         "/steps/0/messages/0/impulse"},
        {"a push with a key it doesn't have", R"("impulse": [0, 0, -40])",
         R"("impulse": [0, 0, -40], "strength": 1)", unknown_key, 12, 124,
         "/steps/0/messages/0/strength"},
        {"a step without messages", R"({"messages": [], )", "{", missing_key, 13, 5,
         "/steps/1/messages"},
        {"a step with an animation pose, which version 1 doesn't have", R"({"messages": [], )",
         R"({"messages": [], "animation_pose": [], )", unknown_key, 13, 22,
         "/steps/1/animation_pose"},
        {"a character in a pose other than the bind pose", R"("bind")", R"("lying")", invalid_value,
         10, 69, "/initial/characters/0/pose"},
        {"a root position of two numbers", "[0, 0.2, 0]", "[0, 0.2]", wrong_length, 10, 48,
         "/initial/characters/0/root_position"},
        {"a world point too large for a float", "1.25", "1e39", number_out_of_range, 12, 87,
         "/steps/0/messages/0/world_point/1"},
        {"characters that aren't an array", R"([{"root_position": [0, 0.2, 0], "pose": "bind"}])",
         "{}", wrong_type, 10, 29, "/initial/characters"},
        {"another format", R"("aplomb.replay")", R"("aplomb.rig")", invalid_value, 2, 13,
         "/format"},
        {"a newer version", R"("version": 1)", R"("version": 2)", unsupported_version, 3, 14,
         "/version"},
    })));
}

TEST_CASE("each mistake in what a replay file describes is one problem, at the value it is with",
          "[files]") {
    constexpr auto invalid_value = aplomb::FileProblemCode::invalid_value;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"a step rate of 0", R"("step_rate": 60)", R"("step_rate": 0)", invalid_value, 7, 16,
         "/step_rate"},
        {"an empty scene", R"("flat_ground")", R"("")", invalid_value, 9, 12, "/scene"},
        {"a push to a character the replay doesn't have", R"("character": 0)", R"("character": 1)",
         invalid_value, 12, 52, "/steps/0/messages/0/character"},
    })));
}

TEST_CASE("every problem in a replay file is reported at once, and its error names them all",
          "[files]") {
    std::string text(pushed_file);
    text.replace(text.find(R"("seed": 7)"), 9, R"("seed": -7)");
    text.replace(text.find(R"("part": 2)"), 9, R"("part": "chest")");

    const auto replay = aplomb::parse_replay_file(std::move(text), "pushed.json");

    REQUIRE_FALSE(replay.has_value());
    REQUIRE(replay.error().problems.size() == 2);
    CHECK(std::format("{}", replay.error()) ==
          "the replay file \"pushed.json\" has 2 problems: pushed.json:8:11: /seed: the seed must "
          "be a whole number from 0 to 18446744073709551615, not -7; pushed.json:12:63: "
          "/steps/0/messages/0/part: expected a number, not a string");
}

TEST_CASE("a saved replay file loads as the replay it was saved from", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_saved_replay_file.json";

    const auto saved = aplomb::save_replay_file(path, pushed_replay());
    const auto loaded = aplomb::load_replay_file(path);

    std::filesystem::remove(path);
    REQUIRE(saved.has_value());
    INFO((loaded.has_value() ? std::string() : std::format("{}", loaded.error())));
    REQUIRE(loaded.has_value());
    CHECK(describe(*loaded) == describe(pushed_replay()));
}

TEST_CASE("a replay file that doesn't exist can't be read, and the error names it", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_replay_file.json";

    const auto replay = aplomb::load_replay_file(path);

    REQUIRE_FALSE(replay.has_value());
    CHECK(replay.error().code == aplomb::FileErrorCode::unreadable);
    CHECK(
        std::format("{}", replay.error()) ==
        std::format("the replay file \"{}\" can't be read: there is no such file", path.string()));
}

TEST_CASE("a replay file in a directory that doesn't exist can't be written, and the error names "
          "it",
          "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_directory" / "replay.json";

    const auto saved = aplomb::save_replay_file(path, pushed_replay());

    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().code == aplomb::FileErrorCode::unwritable);
    CHECK(
        std::format("{}", saved.error()) ==
        std::format("the replay file \"{}\" can't be written: it can't be opened", path.string()));
}
