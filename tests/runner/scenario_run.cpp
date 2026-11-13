#include "scenario_run.hpp"

#include "host_services.hpp"
#include "metric_recorder.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/version.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/core/world_settings.hpp>
#include <aplomb/files/metrics_file.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/scenario_desc.hpp>
#include <aplomb/scenes/build_scene.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

// The scenes there are, as a message lists them: `"a", "b"`.
std::string listed_scenes() {
    std::string list;
    for (const std::string_view name : aplomb::scene_names()) {
        list += std::format("{}\"{}\"", list.empty() ? "" : ", ", name);
    }
    return list;
}

// A world with a scene built into it, and characters made from a rig added in order, so that each
// one's id is its place in `characters`.
struct StartedWorld {
    aplomb::World world;
    std::vector<aplomb::CharacterId> characters;
};

std::expected<StartedWorld, RunError> start_world(std::string_view scene, float step_rate,
                                                  const aplomb::RigDesc& rig,
                                                  std::span<const aplomb::InitialState> characters,
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
    if (!aplomb::build_scene(scene, world->physics_system())) {
        return std::unexpected(RunError{
            .code = RunErrorCode::unknown_scene,
            .message =
                std::format(R"(there is no scene "{}"; the scenes are {})", scene, listed_scenes()),
        });
    }
    StartedWorld started{.world = std::move(*world), .characters = {}};
    for (const aplomb::InitialState& initial : characters) {
        const auto character =
            started.world.add_character(rig, {.position = initial.root_position});
        if (!character.has_value()) {
            return std::unexpected(RunError{
                .code = RunErrorCode::invalid_character,
                .message = std::format("{}", character.error()),
            });
        }
        started.characters.push_back(*character);
    }
    return started;
}

// Reports a push of a replay to a character or a part that it or its rig doesn't have, which the
// world would refuse.
std::expected<void, RunError> check_pushes(const aplomb::ReplayDesc& replay,
                                           const aplomb::RigDesc& rig) {
    for (std::size_t step = 1; step <= replay.steps.size(); ++step) {
        for (const aplomb::Push& push : replay.steps[step - 1].pushes) {
            if (push.character.index() >= replay.initial.characters.size()) {
                return std::unexpected(RunError{
                    .code = RunErrorCode::unknown_character,
                    .message =
                        std::format("step {} of the replay pushes character {}, but the "
                                    "replay has {} character{}",
                                    step, push.character.index(), replay.initial.characters.size(),
                                    replay.initial.characters.size() == 1 ? "" : "s"),
                });
            }
            if (push.part.index() >= rig.parts.size()) {
                return std::unexpected(RunError{
                    .code = RunErrorCode::unknown_part,
                    .message =
                        std::format(R"(step {} of the replay pushes part {}, but the rig "{}" )"
                                    "has {} parts",
                                    step, push.part.index(), rig.name, rig.parts.size()),
                });
            }
        }
    }
    return {};
}

// A push of a scenario's timeline, with the step after which it is sent and the part it pushes.
struct ScheduledPush {
    std::uint64_t step = 0;
    aplomb::PartId part{0};
    JPH::Vec3 point = JPH::Vec3::sZero();
    JPH::Vec3 impulse = JPH::Vec3::sZero();
};

// The timeline's pushes in the order they are sent, or an error naming a part the rig lacks. A push
// nearest the end of a run of `steps` is sent before its last step, since none is sent after it.
std::expected<std::vector<ScheduledPush>, RunError>
schedule_pushes(const aplomb::ScenarioDesc& scenario, const aplomb::RigDesc& rig, float step_rate,
                std::uint64_t steps) {
    std::vector<ScheduledPush> pushes;
    pushes.reserve(scenario.timeline.size());
    for (const aplomb::TimelinePush& push : scenario.timeline) {
        const auto part = std::ranges::find(rig.parts, push.part, &aplomb::PartDesc::name);
        if (part == rig.parts.end()) {
            return std::unexpected(RunError{
                .code = RunErrorCode::unknown_part,
                .message = std::format(R"(the timeline pushes the part "{}", which the rig "{}" )"
                                       "doesn't have",
                                       push.part, rig.name),
            });
        }
        pushes.push_back({
            .step = std::min(steps_in(push.time, step_rate), steps - 1),
            .part = aplomb::PartId(static_cast<std::uint32_t>(part - rig.parts.begin())),
            .point = push.point,
            .impulse = push.impulse,
        });
    }
    std::ranges::stable_sort(pushes, {}, &ScheduledPush::step);
    return pushes;
}

} // namespace

bool RunResult::passed() const noexcept {
    return nan_count == 0 && std::ranges::all_of(criteria, &CriterionResult::passed);
}

std::optional<aplomb::StateHashDifference>
first_differing_step(std::span<const std::uint64_t> first, std::span<const std::uint64_t> second) {
    const auto [first_at, second_at] = std::ranges::mismatch(first, second);
    if (first_at == first.end() || second_at == second.end()) {
        return std::nullopt;
    }
    return aplomb::StateHashDifference{
        .step = static_cast<std::uint64_t>(first_at - first.begin()),
        .state_hashes = {*first_at, *second_at},
    };
}

std::expected<RunResult, RunError> run_scenario(const aplomb::ScenarioDesc& scenario,
                                                const aplomb::RigDesc& rig, float step_rate,
                                                HostServices& services) {
    auto started =
        start_world(scenario.scene, step_rate, rig, std::span(&scenario.initial, 1), services);
    if (!started.has_value()) {
        return std::unexpected(std::move(started.error()));
    }
    aplomb::World& world = started->world;
    const aplomb::CharacterId character = started->characters.front();

    RunResult result;
    result.steps = steps_in(scenario.duration, step_rate);
    const auto pushes = schedule_pushes(scenario, rig, step_rate, result.steps);
    if (!pushes.has_value()) {
        return std::unexpected(pushes.error());
    }
    const JPH::BodyInterface& body_interface = world.physics_system().GetBodyInterface();
    MetricRecorder recorder(step_rate);
    result.state_hashes.reserve(static_cast<std::size_t>(result.steps) + 1);
    result.state_hashes.push_back(world.state_hash());
    world.start_recording();
    auto next_push = pushes->begin();
    for (std::uint64_t step = 0; step < result.steps; ++step) {
        for (; next_push != pushes->end() && next_push->step == step; ++next_push) {
            const JPH::BodyID body = world.character(character).part_body(next_push->part);
            world.push(character, next_push->part,
                       body_interface.GetWorldTransform(body) * next_push->point,
                       next_push->impulse);
        }
        world.step();
        recorder.record(part_states(world, world.character(character)));
    }
    result.recorded_steps = world.stop_recording();
    for (const aplomb::RecordedStep& step : result.recorded_steps) {
        result.state_hashes.push_back(step.state_hash);
    }

    result.metrics = {.scenario = scenario.name, .step_rate = step_rate};
    for (const aplomb::Metric metric : scenario.metrics) {
        result.metrics.values.push_back({
            .metric = metric,
            .value = metric == aplomb::Metric::state_hash_final
                         ? std::variant<double, std::uint64_t>(result.state_hashes.back())
                         : recorder.value(metric),
        });
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

aplomb::ReplayDesc replay_of(const aplomb::ScenarioDesc& scenario, std::uint64_t rig_file_hash,
                             const RunResult& result) {
    return {
        .aplomb_version = std::string(aplomb::version()),
        .jolt_version = std::string(aplomb::jolt_version()),
        .rig_file_hash = rig_file_hash,
        .step_rate = result.metrics.step_rate,
        .seed = scenario.seed,
        .scene = scenario.scene,
        .initial = {.characters = {scenario.initial}, .state_hash = result.state_hashes.front()},
        .steps = result.recorded_steps,
    };
}

std::expected<PlaybackResult, RunError>
play_replay(const aplomb::ReplayDesc& replay, const aplomb::RigDesc& rig, HostServices& services) {
    if (const auto checked = check_pushes(replay, rig); !checked.has_value()) {
        return std::unexpected(checked.error());
    }
    auto started =
        start_world(replay.scene, replay.step_rate, rig, replay.initial.characters, services);
    if (!started.has_value()) {
        return std::unexpected(std::move(started.error()));
    }
    aplomb::World& world = started->world;

    PlaybackResult result;
    std::vector<std::uint64_t> recorded_hashes;
    recorded_hashes.reserve(replay.steps.size() + 1);
    result.state_hashes.reserve(replay.steps.size() + 1);
    recorded_hashes.push_back(replay.initial.state_hash);
    result.state_hashes.push_back(world.state_hash());
    for (const aplomb::RecordedStep& step : replay.steps) {
        for (const aplomb::Push& push : step.pushes) {
            world.push(started->characters[push.character.index()], push.part, push.world_point,
                       push.impulse);
        }
        world.step();
        recorded_hashes.push_back(step.state_hash);
        result.state_hashes.push_back(world.state_hash());
    }
    result.first_differing_step = first_differing_step(recorded_hashes, result.state_hashes);
    return result;
}
