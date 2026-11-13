#include "scenario_run.hpp"

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "required.hpp"
#include "rigs.hpp"
#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/version.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

// The chain rig dropped onto flat ground for 3 s, recording nothing.
aplomb::ScenarioDesc chain_drop() {
    return {
        .name = "chain_drop",
        .rig = "chain.json",
        .scene = "flat_ground",
        .duration = 3.0F,
        .initial = {.root_position = JPH::Vec3(0.0F, 0.2F, 0.0F)},
    };
}

// A push of the chain's pelvis along +X, at its centre.
aplomb::TimelinePush pelvis_push(float time) {
    return {.time = time, .part = "pelvis", .impulse = JPH::Vec3(20.0F, 0.0F, 0.0F)};
}

template <typename T> std::string error_of(const std::expected<T, RunError>& result) {
    return result.has_value() ? std::string() : std::format("{}", result.error());
}

// The steps of a run that applied pushes, counting from 1.
std::vector<std::uint64_t> steps_with_pushes(const RunResult& result) {
    std::vector<std::uint64_t> steps;
    for (std::uint64_t step = 1; const aplomb::RecordedStep& recorded : result.recorded_steps) {
        if (!recorded.pushes.empty()) {
            steps.push_back(step);
        }
        ++step;
    }
    return steps;
}

// The replay of a run of the chain dropped and pushed in the pelvis at 60 Hz.
aplomb::ReplayDesc pushed_chain_replay(HostServices& services) {
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.timeline = {pelvis_push(0.5F)};
    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);
    INFO(error_of(result));
    REQUIRE(result.has_value());
    return replay_of(scenario, 0x0123456789abcdef, *result);
}

} // namespace

TEST_CASE("a run takes the whole number of steps nearest its duration at its step rate",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.duration = 2.5F;

    const auto at_60_hz = run_scenario(scenario, chain_rig(), 60.0F, services);
    const auto at_120_hz = run_scenario(scenario, chain_rig(), 120.0F, services);
    const auto at_59_9_hz = run_scenario(scenario, chain_rig(), 59.9F, services);

    INFO(error_of(at_60_hz));
    REQUIRE(at_60_hz.has_value());
    REQUIRE(at_120_hz.has_value());
    REQUIRE(at_59_9_hz.has_value());
    CHECK(at_60_hz->steps == 150);
    CHECK(at_120_hz->steps == 300);
    CHECK(at_59_9_hz->steps == 150);
}

TEST_CASE("a run records the metrics its scenario lists, in that order, with its name and step "
          "rate",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.metrics = {aplomb::Metric::nan_count, aplomb::Metric::max_speed_after_3s};

    const auto result = run_scenario(scenario, chain_rig(), 120.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    CHECK(result->metrics.scenario == "chain_drop");
    CHECK(result->metrics.step_rate == 120.0F);
    REQUIRE(result->metrics.values.size() == 2);
    CHECK(result->metrics.values[0].metric == aplomb::Metric::nan_count);
    CHECK(result->metrics.values[0].value == std::variant<double, std::uint64_t>(0.0));
    CHECK(result->metrics.values[1].metric == aplomb::Metric::max_speed_after_3s);
    CHECK(std::isfinite(std::get<double>(result->metrics.values[1].value)));
    CHECK(result->nan_count == 0);
}

TEST_CASE("a run passes when it meets every pass criterion", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.metrics = {aplomb::Metric::max_speed_after_3s, aplomb::Metric::nan_count};
    scenario.pass = {
        {
            .metric = aplomb::Metric::nan_count,
            .comparison = aplomb::Comparison::equal,
            .value = 0.0,
        },
        {
            .metric = aplomb::Metric::max_speed_after_3s,
            .comparison = aplomb::Comparison::greater_or_equal,
            .value = 0.0,
        },
    };

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    REQUIRE(result->criteria.size() == 2);
    CHECK(result->criteria[0].criterion.metric == aplomb::Metric::nan_count);
    CHECK(result->criteria[0].passed);
    CHECK(result->criteria[1].criterion.metric == aplomb::Metric::max_speed_after_3s);
    CHECK(result->criteria[1].value == std::get<double>(result->metrics.values[0].value));
    CHECK(result->criteria[1].passed);
    CHECK(result->passed());
}

TEST_CASE("a run fails when it misses a pass criterion", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.metrics = {aplomb::Metric::max_speed_after_3s, aplomb::Metric::nan_count};
    scenario.pass = {
        {
            .metric = aplomb::Metric::nan_count,
            .comparison = aplomb::Comparison::equal,
            .value = 0.0,
        },
        {
            .metric = aplomb::Metric::max_speed_after_3s,
            .comparison = aplomb::Comparison::less,
            .value = -1.0,
        },
    };

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    REQUIRE(result->criteria.size() == 2);
    CHECK(result->criteria[0].passed);
    CHECK_FALSE(result->criteria[1].passed);
    CHECK_FALSE(result->passed());
}

TEST_CASE("a run in which a NaN appears fails, whatever its criteria", "[runner]") {
    const RunResult clean{.nan_count = 0};
    const RunResult lost{.nan_count = 1};

    CHECK(clean.passed());
    CHECK_FALSE(lost.passed());
}

TEST_CASE("a scenario in a scene that doesn't exist can't run, and the error names the scene",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.scene = "moon";

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == RunErrorCode::unknown_scene);
    CHECK(result.error().message == R"(there is no scene "moon"; the scenes are "flat_ground")");
}

TEST_CASE("a scenario can't run at a step rate the world refuses", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;

    const auto result = run_scenario(chain_drop(), chain_rig(), 0.0F, services);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == RunErrorCode::invalid_world);
}

TEST_CASE("a scenario can't run with a rig the world refuses", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;

    const auto result =
        run_scenario(chain_drop(), aplomb::RigDesc{.name = "empty"}, 60.0F, services);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == RunErrorCode::invalid_character);
}

TEST_CASE("a run records the world's state hash before its first step and after every step, the "
          "last as state_hash_final",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.metrics = {aplomb::Metric::state_hash_final};

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    REQUIRE(result->state_hashes.size() == result->steps + 1);
    // The chain falls from the start, so no two steps leave it in the same state.
    CHECK(result->state_hashes[0] != result->state_hashes[1]);
    REQUIRE(result->metrics.values.size() == 1);
    CHECK(result->metrics.values[0].metric == aplomb::Metric::state_hash_final);
    CHECK(result->metrics.values[0].value ==
          std::variant<double, std::uint64_t>(result->state_hashes.back()));
}

TEST_CASE("two runs of a scenario at one step rate give the same state hash at every step",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;

    const auto first = run_scenario(chain_drop(), chain_rig(), 120.0F, services);
    const auto second = run_scenario(chain_drop(), chain_rig(), 120.0F, services);

    INFO(error_of(first));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->state_hashes == second->state_hashes);
    CHECK_FALSE(first_differing_step(first->state_hashes, second->state_hashes).has_value());
}

TEST_CASE("the first differing step of two runs is the first at which their state hashes differ, "
          "with both hashes",
          "[runner]") {
    const std::vector<std::uint64_t> first{1, 2, 3, 4, 5};
    const std::vector<std::uint64_t> second{1, 2, 30, 4, 50};

    const std::optional<aplomb::StateHashDifference> found = first_differing_step(first, second);

    const aplomb::StateHashDifference& difference = required(found);
    CHECK(difference.step == 2);
    CHECK(difference.state_hashes == std::array<std::uint64_t, 2>{3, 30});
}

TEST_CASE("a run sends each push of its timeline after the whole number of steps nearest its time",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc pushed = chain_drop();
    pushed.timeline = {pelvis_push(0.5F)};

    const auto with_push = run_scenario(pushed, chain_rig(), 60.0F, services);
    const auto without = run_scenario(chain_drop(), chain_rig(), 60.0F, services);

    INFO(error_of(with_push));
    REQUIRE(with_push.has_value());
    REQUIRE(without.has_value());
    // Sent after 30 steps, the push takes effect at the 31st.
    const std::optional<aplomb::StateHashDifference> difference =
        first_differing_step(with_push->state_hashes, without->state_hashes);
    CHECK(required(difference).step == 31);
}

TEST_CASE("a run sends its timeline's pushes in the order of their times, whatever the order "
          "they are listed in",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::TimelinePush later = pelvis_push(1.0F);
    later.part = "shin";
    later.impulse = JPH::Vec3(0.0F, 0.0F, -6.0F);
    aplomb::ScenarioDesc in_order = chain_drop();
    in_order.timeline = {pelvis_push(0.5F), later};
    aplomb::ScenarioDesc out_of_order = chain_drop();
    out_of_order.timeline = {later, pelvis_push(0.5F)};

    const auto first = run_scenario(in_order, chain_rig(), 60.0F, services);
    const auto second = run_scenario(out_of_order, chain_rig(), 60.0F, services);

    INFO(error_of(first));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->state_hashes == second->state_hashes);
}

TEST_CASE("a timeline push is applied at its point in the part's space, as the part is when the "
          "push is sent",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    // The pelvis turned a half turn about Z, which takes its +Y to the world's -Y exactly.
    aplomb::RigDesc rig = chain_rig();
    rig.parts[0].bind_rotation = JPH::Quat(0.0F, 0.0F, 1.0F, 0.0F);
    aplomb::ScenarioDesc scenario = chain_drop();
    aplomb::TimelinePush push = pelvis_push(0.0F);
    push.point = JPH::Vec3(0.0F, 0.1F, 0.0F);
    scenario.timeline = {push};

    const auto result = run_scenario(scenario, rig, 60.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    // The same run, with the push sent to the world by hand before the first step, while the
    // pelvis is in its bind pose: 0.95 m up in the rig, 0.2 m up in the world.
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    aplomb::build_flat_ground(world->physics_system());
    const JPH::Vec3 root = scenario.initial.root_position;
    const auto character = world->add_character(rig, {.position = root});
    REQUIRE(character.has_value());
    world->push(*character, aplomb::PartId(0),
                root + JPH::Vec3(0.0F, 0.95F, 0.0F) + JPH::Vec3(0.0F, -0.1F, 0.0F), push.impulse);
    for (std::uint64_t step = 1; step <= result->steps; ++step) {
        world->step();
        INFO("step " << step);
        REQUIRE(world->state_hash() == result->state_hashes[step]);
    }
}

TEST_CASE("a scenario that pushes a part its rig doesn't have can't run, and the error names the "
          "part",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    aplomb::TimelinePush push = pelvis_push(0.5F);
    push.part = "chest";
    scenario.timeline = {push};

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == RunErrorCode::unknown_part);
    CHECK(std::format("{}", result.error()) ==
          R"(the timeline pushes the part "chest", which the rig "chain" doesn't have)");
}

TEST_CASE("a push within half a step of the end of its run is sent before the last step",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc pushed = chain_drop();
    pushed.duration = 1.0F;
    // 59.7 steps into a run of 60, which is nearest the 60th, after which nothing is sent.
    pushed.timeline = {pelvis_push(0.995F)};
    aplomb::ScenarioDesc unpushed = chain_drop();
    unpushed.duration = 1.0F;

    const auto with_push = run_scenario(pushed, chain_rig(), 60.0F, services);
    const auto without = run_scenario(unpushed, chain_rig(), 60.0F, services);

    INFO(error_of(with_push));
    REQUIRE(with_push.has_value());
    REQUIRE(without.has_value());
    const std::optional<aplomb::StateHashDifference> difference =
        first_differing_step(with_push->state_hashes, without->state_hashes);
    CHECK(required(difference).step == 60);
}

TEST_CASE("a run records each step's pushes and its state hash after it", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.timeline = {pelvis_push(0.5F)};

    const auto result = run_scenario(scenario, chain_rig(), 60.0F, services);

    INFO(error_of(result));
    REQUIRE(result.has_value());
    std::vector<std::uint64_t> recorded_hashes{result->state_hashes.front()};
    for (const aplomb::RecordedStep& recorded : result->recorded_steps) {
        recorded_hashes.push_back(recorded.state_hash);
    }
    CHECK(recorded_hashes == result->state_hashes);
    // Sent after 30 steps, the push is applied at the start of the 31st.
    REQUIRE(steps_with_pushes(*result) == std::vector<std::uint64_t>{31});
    REQUIRE(result->recorded_steps[30].pushes.size() == 1);
    const aplomb::Push& push = result->recorded_steps[30].pushes[0];
    CHECK(push.part == aplomb::PartId(0));
    CHECK(push.impulse == JPH::Vec3(20.0F, 0.0F, 0.0F));
}

TEST_CASE("a run's replay holds the versions, the rig file's hash, the step rate, the seed, the "
          "scene and the initial state it ran from, and each step it took",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.seed = 42;
    scenario.timeline = {pelvis_push(0.5F)};
    const auto result = run_scenario(scenario, chain_rig(), 120.0F, services);
    INFO(error_of(result));
    REQUIRE(result.has_value());

    const aplomb::ReplayDesc replay = replay_of(scenario, 0x0123456789abcdef, *result);

    CHECK(replay.aplomb_version == aplomb::version());
    CHECK(replay.jolt_version == aplomb::jolt_version());
    CHECK(replay.rig_file_hash == 0x0123456789abcdef);
    CHECK(replay.step_rate == 120.0F);
    CHECK(replay.seed == 42);
    CHECK(replay.scene == "flat_ground");
    REQUIRE(replay.initial.characters.size() == 1);
    CHECK(replay.initial.characters[0].root_position == scenario.initial.root_position);
    CHECK(replay.initial.state_hash == result->state_hashes[0]);
    CHECK(replay.steps == result->recorded_steps);
}

TEST_CASE("a run's replay plays back to the same state hash at every step", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ScenarioDesc scenario = chain_drop();
    scenario.timeline = {pelvis_push(0.5F)};
    const auto run = run_scenario(scenario, chain_rig(), 60.0F, services);
    INFO(error_of(run));
    REQUIRE(run.has_value());

    const auto played = play_replay(replay_of(scenario, 0, *run), chain_rig(), services);

    INFO(error_of(played));
    REQUIRE(played.has_value());
    CHECK(played->state_hashes == run->state_hashes);
    CHECK_FALSE(played->first_differing_step.has_value());
}

TEST_CASE("a replay whose recorded state hash differs from the played back one fails at the "
          "first step where they differ, with both hashes",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    const std::uint64_t played_back = replay.steps[40].state_hash;
    replay.steps[40].state_hash = 1;
    replay.steps[50].state_hash = 2;

    const auto played = play_replay(replay, chain_rig(), services);

    INFO(error_of(played));
    REQUIRE(played.has_value());
    const aplomb::StateHashDifference& difference = required(played->first_differing_step);
    CHECK(difference.step == 41);
    CHECK(difference.state_hashes == std::array<std::uint64_t, 2>{1, played_back});
}

TEST_CASE("a replay whose initial state hash differs from the played back one fails before the "
          "first step",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    replay.initial.state_hash = 1;

    const auto played = play_replay(replay, chain_rig(), services);

    INFO(error_of(played));
    REQUIRE(played.has_value());
    CHECK(required(played->first_differing_step).step == 0);
}

TEST_CASE("a replay sends its pushes, so one played back without its push differs from the step "
          "the push was applied at",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    replay.steps[30].pushes.clear();

    const auto played = play_replay(replay, chain_rig(), services);

    INFO(error_of(played));
    REQUIRE(played.has_value());
    CHECK(required(played->first_differing_step).step == 31);
}

TEST_CASE("a replay that pushes a part its rig doesn't have can't be played back, and the error "
          "names the step and the part",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    replay.steps[30].pushes[0].part = aplomb::PartId(3);

    const auto played = play_replay(replay, chain_rig(), services);

    REQUIRE_FALSE(played.has_value());
    CHECK(played.error().code == RunErrorCode::unknown_part);
    CHECK(std::format("{}", played.error()) ==
          R"(step 31 of the replay pushes part 3, but the rig "chain" has 3 parts)");
}

TEST_CASE("a replay that pushes a character it doesn't have can't be played back, and the error "
          "names the step and the character",
          "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    replay.steps[30].pushes[0].character = aplomb::CharacterId(1);

    const auto played = play_replay(replay, chain_rig(), services);

    REQUIRE_FALSE(played.has_value());
    CHECK(played.error().code == RunErrorCode::unknown_character);
    CHECK(std::format("{}", played.error()) ==
          "step 31 of the replay pushes character 1, but the replay has 1 character");
}

TEST_CASE("a replay of a scene that doesn't exist can't be played back", "[runner]") {
    const JoltRuntime runtime;
    HostServices services;
    aplomb::ReplayDesc replay = pushed_chain_replay(services);
    replay.scene = "moon";

    const auto played = play_replay(replay, chain_rig(), services);

    REQUIRE_FALSE(played.has_value());
    CHECK(played.error().code == RunErrorCode::unknown_scene);
}
