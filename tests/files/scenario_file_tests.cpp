#include <aplomb/files/scenario_file.hpp>

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// A scenario file that drops a rig onto flat ground. The line numbers below count from the opening
// brace.
constexpr std::string_view drop_file = R"({
  "format": "aplomb.scenario",
  "version": 1,
  "name": "drop",
  "rig": "../rigs/chain.json",
  "scene": "flat_ground",
  "duration_s": 5,
  "seed": 7,
  "initial": {"root_position": [0, 0.2, 0], "pose": "bind"},
  "timeline": [],
  "metrics": ["max_speed_after_3s", "nan_count"],
  "pass": {
    "max_speed_after_3s": {"lt": 0.01},
    "nan_count": {"ge": 0, "le": 0}
  }
}
)";

// The drop file with some of its text replaced, each `from` a text it has once.
std::string
drop_file_with(std::initializer_list<std::pair<std::string_view, std::string_view>> replacements) {
    std::string text(drop_file);
    for (const auto& [from, to] : replacements) {
        const std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        REQUIRE(text.find(from, at + 1) == std::string::npos);
        text.replace(at, from.size(), to);
    }
    return text;
}

// One mistake made in the drop file, by replacing `from` with `to`, and the one problem it must
// give.
struct Mistake {
    std::string_view description;
    std::string_view from;
    std::string_view to;
    aplomb::FileProblemCode code;
    std::size_t line;
    std::size_t column;
    std::string_view pointer;
};

// Where a problem is, as `line:column pointer`, so that a failed check shows all of it.
std::string place_of(const aplomb::FileProblem& problem) {
    return std::format("{}:{} {}", problem.line, problem.column, problem.pointer);
}

std::vector<std::string> places_of(const std::vector<aplomb::FileProblem>& problems) {
    std::vector<std::string> places;
    places.reserve(problems.size());
    for (const aplomb::FileProblem& problem : problems) {
        places.push_back(place_of(problem));
    }
    return places;
}

// Each thing a scenario holds, one to a line, so that a failed check shows all of it.
std::vector<std::string> describe(const aplomb::ScenarioDesc& scenario) {
    const JPH::Vec3 root = scenario.initial.root_position;
    std::vector<std::string> lines{
        std::format("name {}", scenario.name),
        std::format("rig {}", scenario.rig.generic_string()),
        std::format("scene {}", scenario.scene),
        std::format("duration {}", scenario.duration),
        std::format("seed {}", scenario.seed),
        std::format("root position [{}, {}, {}]", root.GetX(), root.GetY(), root.GetZ()),
        scenario.initial.pose == aplomb::InitialPose::bind ? "bind pose" : "another pose",
    };
    for (const aplomb::Metric metric : scenario.metrics) {
        lines.push_back(std::format("metric {}", aplomb::metric_name(metric)));
    }
    for (const aplomb::PassCriterion& criterion : scenario.pass) {
        lines.push_back(std::format("pass {} {} {}", aplomb::metric_name(criterion.metric),
                                    aplomb::comparison_name(criterion.comparison),
                                    criterion.value));
    }
    return lines;
}

void check_one_problem(const Mistake& mistake) {
    INFO("a scenario file with " << mistake.description);
    const auto scenario =
        aplomb::parse_scenario_file(drop_file_with({{mistake.from, mistake.to}}), "drop.json");

    REQUIRE_FALSE(scenario.has_value());
    REQUIRE(scenario.error().problems.size() == 1);
    const aplomb::FileProblem& problem = scenario.error().problems.front();
    INFO(std::format("{}", problem));
    CHECK(problem.code == mistake.code);
    CHECK(std::format("{} {}", problem.path, place_of(problem)) ==
          std::format("drop.json {}:{} {}", mistake.line, mistake.column, mistake.pointer));
}

} // namespace

TEST_CASE("a scenario file is read into the scenario it describes", "[files]") {
    const auto scenario = aplomb::parse_scenario_file(std::string(drop_file), "drop.json");

    INFO((scenario.has_value() ? std::string() : std::format("{}", scenario.error())));
    REQUIRE(scenario.has_value());
    CHECK(describe(*scenario) == std::vector<std::string>{
                                     "name drop",
                                     "rig ../rigs/chain.json",
                                     "scene flat_ground",
                                     "duration 5",
                                     "seed 7",
                                     "root position [0, 0.2, 0]",
                                     "bind pose",
                                     "metric max_speed_after_3s",
                                     "metric nan_count",
                                     "pass max_speed_after_3s lt 0.01",
                                     "pass nan_count ge 0",
                                     "pass nan_count le 0",
                                 });
}

TEST_CASE("each mistake in a scenario file's syntax or structure is one problem, with its line, "
          "column and JSON pointer",
          "[files]") {
    using enum aplomb::FileProblemCode;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"a stray comma", R"("seed": 7,)", R"("seed": 7,,)", syntax_error, 8, 13, ""},
        {"a key given twice", R"("seed": 7,)", R"("seed": 7, "seed": 8,)", duplicate_key, 8, 14,
         "/seed"},
        {"a step rate, which the runner chooses", R"("seed": 7,)", R"("seed": 7, "step_rate": 60,)",
         unknown_key, 8, 14, "/step_rate"},
        {"a key the initial state doesn't have", R"("pose": "bind")",
         R"("pose": "bind", "speed": 1)", unknown_key, 9, 61, "/initial/speed"},
        {"no name", "  \"name\": \"drop\",\n", "", missing_key, 1, 1, "/name"},
        {"an initial state without a pose", R"(, "pose": "bind")", "", missing_key, 9, 14,
         "/initial/pose"},
        {"a duration that is a string", R"("duration_s": 5)", R"("duration_s": "5")", wrong_type, 7,
         17, "/duration_s"},
        {"a seed with a fraction", R"("seed": 7)", R"("seed": 7.5)", invalid_value, 8, 11, "/seed"},
        {"a negative seed", R"("seed": 7)", R"("seed": -7)", invalid_value, 8, 11, "/seed"},
        {"a root position of two numbers", "[0, 0.2, 0]", "[0, 0.2]", wrong_length, 9, 32,
         "/initial/root_position"},
        {"an initial pose other than the bind pose", R"("bind")", R"("lying")", invalid_value, 9,
         53, "/initial/pose"},
        {"a metric that doesn't exist", R"("nan_count"])", R"("nan_count", "speed"])",
         invalid_value, 11, 50, "/metrics/2"},
        {"a metric listed twice", R"("nan_count"])", R"("nan_count", "nan_count"])", invalid_value,
         11, 50, "/metrics/2"},
        {"metrics that aren't an array", R"(["max_speed_after_3s", "nan_count"])", R"("nan_count")",
         wrong_type, 11, 14, "/metrics"},
        {"a message in the timeline", R"("timeline": [])",
         R"("timeline": [{"t": 1, "message": "push"}])", invalid_value, 10, 36,
         "/timeline/0/message"},
        {"a timeline entry without a message", R"("timeline": [])", R"("timeline": [{"t": 1}])",
         missing_key, 10, 16, "/timeline/0/message"},
        {"a pass criterion of a metric that doesn't exist", R"("le": 0})",
         R"("le": 0}, "speed": {"lt": 1})", unknown_key, 14, 38, "/pass/speed"},
        {"a comparison that doesn't exist", R"("le": 0})", R"("lte": 0})", unknown_key, 14, 28,
         "/pass/nan_count/lte"},
        {"a pass criterion that compares with nothing", R"({"lt": 0.01})", "{}", invalid_value, 13,
         27, "/pass/max_speed_after_3s"},
        {"a pass criterion's value that is a string", R"({"lt": 0.01})", R"({"lt": "0.01"})",
         wrong_type, 13, 34, "/pass/max_speed_after_3s/lt"},
        {"another format", R"("aplomb.scenario")", R"("aplomb.rig")", invalid_value, 2, 13,
         "/format"},
        {"a newer version", R"("version": 1)", R"("version": 2)", unsupported_version, 3, 14,
         "/version"},
    })));
}

TEST_CASE("each mistake in what a scenario file describes is one problem, at the value it is with",
          "[files]") {
    constexpr auto invalid_value = aplomb::FileProblemCode::invalid_value;
    check_one_problem(GENERATE(Catch::Generators::values<Mistake>({
        {"a duration of 0", R"("duration_s": 5)", R"("duration_s": 0)", invalid_value, 7, 17,
         "/duration_s"},
        {"max_speed_after_3s in a run shorter than 3 s", R"("duration_s": 5)",
         R"("duration_s": 2.5)", invalid_value, 11, 15, "/metrics/0"},
        {"an empty rig path", R"("../rigs/chain.json")", R"("")", invalid_value, 5, 10, "/rig"},
        {"an empty scene", R"("flat_ground")", R"("")", invalid_value, 6, 12, "/scene"},
        {"a pass criterion of a metric that isn't recorded",
         R"(["max_speed_after_3s", "nan_count"])", R"(["max_speed_after_3s"])", invalid_value, 14,
         18, "/pass/nan_count"},
    })));
}

TEST_CASE("a run of exactly 3 s is long enough to record max_speed_after_3s", "[files]") {
    const auto scenario = aplomb::parse_scenario_file(
        drop_file_with({{R"("duration_s": 5)", R"("duration_s": 3)"}}), "drop.json");

    CHECK(scenario.has_value());
}

TEST_CASE("every problem in a scenario file's structure is reported at once, in the order of the "
          "file, and none in its meaning",
          "[files]") {
    const auto scenario =
        aplomb::parse_scenario_file(drop_file_with({
                                        {R"("le": 0})", R"("lte": 0})"},
                                        {R"("duration_s": 5)", R"("duration_s": 0)"},
                                        {R"("bind")", R"("lying")"},
                                        {R"("seed": 7)", R"("seed": "7")"},
                                    }),
                                    "drop.json");

    REQUIRE_FALSE(scenario.has_value());
    CHECK(places_of(scenario.error().problems) == std::vector<std::string>{
                                                      "8:11 /seed",
                                                      "9:53 /initial/pose",
                                                      "14:28 /pass/nan_count/lte",
                                                  });
}

TEST_CASE("every problem in what a scenario file describes is reported at once, in the order of "
          "the file",
          "[files]") {
    const auto scenario = aplomb::parse_scenario_file(
        drop_file_with({
            {R"(["max_speed_after_3s", "nan_count"])", R"(["max_speed_after_3s"])"},
            {R"("flat_ground")", R"("")"},
            {R"("duration_s": 5)", R"("duration_s": 1)"},
        }),
        "drop.json");

    REQUIRE_FALSE(scenario.has_value());
    CHECK(places_of(scenario.error().problems) == std::vector<std::string>{
                                                      "6:12 /scene",
                                                      "11:15 /metrics/0",
                                                      "14:18 /pass/nan_count",
                                                  });
}

TEST_CASE("a scenario file of another format is read no further", "[files]") {
    const auto scenario =
        aplomb::parse_scenario_file(drop_file_with({
                                        {R"("aplomb.scenario")", R"("aplomb.rig")"},
                                        {R"("le": 0})", R"("lte": 0})"},
                                    }),
                                    "drop.json");

    REQUIRE_FALSE(scenario.has_value());
    CHECK(places_of(scenario.error().problems) == std::vector<std::string>{"2:13 /format"});
}

TEST_CASE("a scenario file's problems read as a compiler's diagnostics, and its error names them "
          "all",
          "[files]") {
    const auto scenario = aplomb::parse_scenario_file(drop_file_with({
                                                          {R"("nan_count"])", R"("speed"])"},
                                                          {R"({"lt": 0.01})", "{}"},
                                                      }),
                                                      "scenarios/drop.json");

    REQUIRE_FALSE(scenario.has_value());
    CHECK(std::format("{}", scenario.error()) ==
          "the scenario file \"scenarios/drop.json\" has 2 problems: "
          "scenarios/drop.json:11:37: /metrics/1: a metric is \"max_speed_after_3s\" or "
          "\"nan_count\", not \"speed\"; "
          "scenarios/drop.json:13:27: /pass/max_speed_after_3s: a pass criterion compares its "
          "metric with \"lt\", \"le\", \"eq\", \"ge\" or \"gt\"");
}

TEST_CASE("a scenario file that doesn't exist can't be read, and the error names it", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_scenario_file.json";

    const auto scenario = aplomb::load_scenario_file(path);

    REQUIRE_FALSE(scenario.has_value());
    CHECK(scenario.error().code == aplomb::FileErrorCode::unreadable);
    CHECK(std::format("{}", scenario.error()) ==
          std::format("the scenario file \"{}\" can't be read: there is no such file",
                      path.string()));
}
