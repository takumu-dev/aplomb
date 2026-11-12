#include "scenario_run.hpp"

#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "required.hpp"
#include "rigs.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <Jolt/Jolt.h>

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

std::string error_of(const std::expected<RunResult, RunError>& result) {
    return result.has_value() ? std::string() : std::format("{}", result.error());
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
