#include "scenario_run.hpp"

#include "host_services.hpp"
#include "metric_recorder.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/scenes/build_scene.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace {

// The scenes there are, as a message lists them: `"a", "b"`.
std::string listed_scenes() {
    std::string list;
    for (const std::string_view name : aplomb::scene_names()) {
        list += std::format("{}\"{}\"", list.empty() ? "" : ", ", name);
    }
    return list;
}

} // namespace

bool RunResult::passed() const noexcept {
    return nan_count == 0 && std::ranges::all_of(criteria, &CriterionResult::passed);
}

std::expected<RunResult, RunError> run_scenario(const aplomb::ScenarioDesc& scenario,
                                                const aplomb::RigDesc& rig, float step_rate,
                                                HostServices& services) {
    aplomb::WorldSettings settings = services.world_settings();
    settings.step_rate = step_rate;
    auto world = aplomb::World::create(settings);
    if (!world.has_value()) {
        return std::unexpected(RunError{
            .code = RunErrorCode::invalid_world,
            .message = std::format("{}", world.error()),
        });
    }
    if (!aplomb::build_scene(scenario.scene, world->physics_system())) {
        return std::unexpected(RunError{
            .code = RunErrorCode::unknown_scene,
            .message = std::format(R"(there is no scene "{}"; the scenes are {})", scenario.scene,
                                   listed_scenes()),
        });
    }
    const auto character = world->add_character(rig, {.position = scenario.initial.root_position});
    if (!character.has_value()) {
        return std::unexpected(RunError{
            .code = RunErrorCode::invalid_character,
            .message = std::format("{}", character.error()),
        });
    }

    RunResult result;
    result.steps = steps_in(scenario.duration, step_rate);
    MetricRecorder recorder(step_rate);
    for (std::uint64_t step = 0; step < result.steps; ++step) {
        world->step();
        recorder.record(part_states(*world, world->character(*character)));
    }

    result.metrics = {.scenario = scenario.name, .step_rate = step_rate, .values = {}};
    for (const aplomb::Metric metric : scenario.metrics) {
        result.metrics.values.push_back({.metric = metric, .value = recorder.value(metric)});
    }
    result.nan_count = static_cast<std::uint64_t>(recorder.value(aplomb::Metric::nan_count));
    for (const aplomb::PassCriterion& criterion : scenario.pass) {
        const double value = recorder.value(criterion.metric);
        result.criteria.push_back({
            .criterion = criterion,
            .value = value,
            .passed = aplomb::compare(criterion.comparison, value, criterion.value),
        });
    }
    return result;
}
