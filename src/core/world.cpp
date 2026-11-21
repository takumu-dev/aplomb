#include <aplomb/core/world.hpp>

#include <aplomb/core/assertion.hpp>
#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/log_sink.hpp>
#include <aplomb/core/object_layers.hpp>
#include <aplomb/core/push.hpp>
#include <aplomb/core/recorded_step.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Core/Factory.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/StateRecorder.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace aplomb {

namespace {

// How much the owned physics system makes room for. Ten characters of sixteen parts each, lying in
// a heap on the scene, stay well inside these.
constexpr JPH::uint max_bodies = 1024;
constexpr JPH::uint body_mutexes = 0; // Jolt's default
constexpr JPH::uint max_body_pairs = 65536;
constexpr JPH::uint max_contact_constraints = 10240;

// Each object layer has a broad-phase layer of its own, so that the static scene sits in a tree
// apart from the bodies that move.
namespace broad_phase_layers {

constexpr JPH::BroadPhaseLayer scene(0);
constexpr JPH::BroadPhaseLayer moving(1);
constexpr JPH::uint count = 2;

} // namespace broad_phase_layers

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
  public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return broad_phase_layers::count;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == object_layers::scene ? broad_phase_layers::scene
                                             : broad_phase_layers::moving;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == broad_phase_layers::scene ? "scene" : "moving";
    }
#endif
};

// The static scene collides only with what moves.
class ObjectVsBroadPhaseLayers final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object_layer,
                                     JPH::BroadPhaseLayer broad_phase_layer) const override {
        return object_layer != object_layers::scene ||
               broad_phase_layer != broad_phase_layers::scene;
    }
};

class ObjectLayerPairs final : public JPH::ObjectLayerPairFilter {
  public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
                                     JPH::ObjectLayer second) const override {
        return first != object_layers::scene || second != object_layers::scene;
    }
};

// Names what a step ran out of room for, such as "body pairs and contact constraints".
std::string describe_shortage(JPH::EPhysicsUpdateError errors) {
    std::string shortage;
    const auto append_if_set = [&](JPH::EPhysicsUpdateError error, std::string_view what) {
        if ((errors & error) != JPH::EPhysicsUpdateError::None) {
            shortage += shortage.empty() ? "" : " and ";
            shortage += what;
        }
    };
    append_if_set(JPH::EPhysicsUpdateError::BodyPairCacheFull, "body pairs");
    append_if_set(JPH::EPhysicsUpdateError::ManifoldCacheFull, "contact manifolds");
    append_if_set(JPH::EPhysicsUpdateError::ContactConstraintsFull, "contact constraints");
    return shortage;
}

// A state recorder that keeps nothing of what is saved to it but its 64-bit FNV-1a hash, whose
// constants are fixed by its definition rather than by any library's version. Jolt saves values in
// the platform's byte order, which is little-endian on every platform aplomb supports.
class StateHasher final : public JPH::StateRecorder {
  public:
    [[nodiscard]] std::uint64_t hash() const noexcept { return hash_; }

    void WriteBytes(const void* data, std::size_t size) override {
        for (const std::byte byte : std::span(static_cast<const std::byte*>(data), size)) {
            hash_ = (hash_ ^ std::to_integer<std::uint64_t>(byte)) * prime;
        }
    }

    void ReadBytes(void* /*data*/, std::size_t /*size*/) override {
        APLOMB_ASSERT(false, "a state hasher is only written to");
    }

    [[nodiscard]] bool IsEOF() const override { return false; }

    [[nodiscard]] bool IsFailed() const override { return false; }

  private:
    static constexpr std::uint64_t offset_basis = 0xcbf29ce484222325;
    static constexpr std::uint64_t prime = 0x100000001b3;

    std::uint64_t hash_ = offset_basis;
};

// The updates a physics system took, and how long they stepped it for in all.
struct TimedUpdates {
    int count = 0;
    float elapsed = 0.0F;
};

// Counts the updates of the physics system it listens to, and adds up how long they step it for
// through each of their collision steps. Jolt calls it once per collision step, one step after
// another, and an update returns only once its steps are done, so what it counts is read safely
// after the update.
class StepTimer final : public JPH::PhysicsStepListener {
  public:
    // The updates since this was last called.
    [[nodiscard]] TimedUpdates take() noexcept { return std::exchange(updates_, {}); }

    void OnStep(const JPH::PhysicsStepListenerContext& step) override {
        updates_.count += step.mIsFirstStep ? 1 : 0;
        updates_.elapsed += step.mDeltaTime;
    }

  private:
    TimedUpdates updates_;
};

// Whether `elapsed`, how long a host stepped its physics system for between the two calls around
// its update, is as long as one of the world's steps. Collision steps' durations add up to their
// update's only to within rounding.
bool lasted_one_step(float elapsed, float step_duration) noexcept {
    constexpr float tolerance = 1e-4F;
    return std::abs(elapsed - step_duration) <= step_duration * tolerance;
}

} // namespace

// An owned physics system keeps pointers to the layer filters, so the state never moves once made.
// The characters come after the physics system, so that they leave it before it is destroyed. The
// fields are ordered so that none needs padding.
struct World::State {
    BroadPhaseLayers broad_phase_layers;
    ObjectVsBroadPhaseLayers object_vs_broad_phase_layers;
    ObjectLayerPairs object_layer_pairs;
    // The owned physics system, or the host's for an attached world.
    JPH::PhysicsSystem* physics_system = nullptr;
    // Empty for an attached world.
    std::optional<JPH::PhysicsSystem> owned_physics_system;
    JPH::JobSystem* job_system = nullptr;
    JPH::TempAllocator* temp_allocator = nullptr;
    // Indexed by character id. A removed character leaves its place empty, so that its id is never
    // given to another. Each character lives apart from the table, so that adding another doesn't
    // move it from under a host's reference.
    std::vector<std::unique_ptr<Character>> characters;
    // The pushes sent since the last step, in the order they were sent.
    std::vector<Push> pushes;
    std::vector<RecordedStep> recorded_steps;
    // Listens to an attached world's physics system, to check the host's step.
    StepTimer step_timer;
    float step_duration = 0.0F;
    JPH::ObjectLayer part_layer = object_layers::moving;
    bool recording = false;
    // Whether the host is between an attached world's before_physics_step() and
    // after_physics_step().
    bool in_host_step = false;

    State() = default;
    State(const State&) = delete;
    State(State&&) = delete;
    State& operator=(const State&) = delete;
    State& operator=(State&&) = delete;

    ~State() {
        if (attached()) {
            physics_system->RemoveStepListener(&step_timer);
        }
    }

    [[nodiscard]] bool attached() const noexcept { return !owned_physics_system.has_value(); }
};

std::expected<World, WorldError> World::create(const WorldSettings& settings) {
    APLOMB_ASSERT(JPH::Factory::sInstance != nullptr,
                  "Jolt's factory exists before a world is created");
    // RegisterTypes() makes the default physics material, and UnregisterTypes() releases it.
    APLOMB_ASSERT(JPH::PhysicsMaterial::sDefault != nullptr,
                  "Jolt's types are registered before a world is created");
    APLOMB_ASSERT(settings.job_system != nullptr, "a world is given a job system");
    APLOMB_ASSERT(settings.temp_allocator != nullptr, "a world is given a temporary allocator");
    APLOMB_ASSERT(settings.physics_system != nullptr || !settings.part_layer.has_value(),
                  "a world that owns its physics system is given no object layer for parts");

    // The rate is checked before it divides anything. One so small that a step would last forever
    // fails too.
    if (!(settings.step_rate > 0.0F) || !std::isfinite(settings.step_rate) ||
        !std::isfinite(1.0F / settings.step_rate)) {
        return std::unexpected(WorldError{
            .code = WorldErrorCode::invalid_step_rate,
            .message = std::format(
                "a world's step rate must be a positive, finite number of steps a second, not {}",
                settings.step_rate),
        });
    }

    auto state = std::make_unique<State>();
    if (settings.physics_system != nullptr) {
        APLOMB_ASSERT(settings.part_layer.has_value(),
                      "an attached world is given an object layer for its characters' parts");
        state->physics_system = settings.physics_system;
        state->part_layer = *settings.part_layer;
        state->physics_system->AddStepListener(&state->step_timer);
    } else {
        state->physics_system = &state->owned_physics_system.emplace();
        state->physics_system->Init(max_bodies, body_mutexes, max_body_pairs,
                                    max_contact_constraints, state->broad_phase_layers,
                                    state->object_vs_broad_phase_layers, state->object_layer_pairs);
    }
    state->step_duration = 1.0F / settings.step_rate;
    state->job_system = settings.job_system;
    state->temp_allocator = settings.temp_allocator;
    return World(std::move(state));
}

World::World(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {}

World::World(World&& other) noexcept = default;

World& World::operator=(World&& other) noexcept = default;

World::~World() = default;

void World::step() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(!state_->attached(), "the world owns its physics system");
    RecordedStep recorded = apply_pushes();
    constexpr int collision_steps = 1;
    const JPH::EPhysicsUpdateError errors = state_->physics_system->Update(
        state_->step_duration, collision_steps, state_->temp_allocator, state_->job_system);
    // Jolt carries on without what didn't fit, so some collisions go unhandled for this step.
    if (errors != JPH::EPhysicsUpdateError::None) {
        log(LogSeverity::warning,
            std::format("a step ran out of room in the physics system for {}, and some collisions "
                        "were missed",
                        describe_shortage(errors)));
    }
    finish_step(std::move(recorded));
}

void World::before_physics_step() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(state_->attached(), "the world is attached to the host's physics system");
    APLOMB_ASSERT(!state_->in_host_step, "the host's last step ended with after_physics_step()");
    const TimedUpdates since_last_step = state_->step_timer.take();
    APLOMB_ASSERT(since_last_step.count == 0,
                  "the host steps its physics system only between before_physics_step() and "
                  "after_physics_step()");
    state_->in_host_step = true;
    std::ignore = apply_pushes();
}

void World::after_physics_step() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(state_->attached(), "the world is attached to the host's physics system");
    APLOMB_ASSERT(state_->in_host_step, "the host's step began with before_physics_step()");
    state_->in_host_step = false;
    const TimedUpdates during_step = state_->step_timer.take();
    APLOMB_ASSERT(during_step.count == 1 &&
                      lasted_one_step(during_step.elapsed, state_->step_duration),
                  "the host steps its physics system once by the world's step duration");
    finish_step({});
}

RecordedStep World::apply_pushes() {
    JPH::BodyInterface& body_interface = state_->physics_system->GetBodyInterface();
    // A recorded step holds the pushes that were applied, so that a push dropped with its
    // character isn't sent to a character that a replay of the step doesn't have.
    RecordedStep recorded;
    for (const Push& push : state_->pushes) {
        if (const std::unique_ptr<Character>& character =
                state_->characters[push.character.index()];
            character != nullptr) {
            body_interface.AddImpulse(character->part_body(push.part), push.impulse,
                                      push.world_point);
            if (state_->recording) {
                recorded.pushes.push_back(push);
            }
        }
    }
    state_->pushes.clear();
    return recorded;
}

void World::finish_step(RecordedStep recorded) {
    const JPH::BodyInterface& body_interface = state_->physics_system->GetBodyInterface();
    for (const std::unique_ptr<Character>& character : state_->characters) {
        if (character != nullptr) {
            character->update_physics_pose(body_interface);
        }
    }
    if (state_->recording) {
        recorded.state_hash = state_hash();
        state_->recorded_steps.push_back(std::move(recorded));
    }
}

void World::start_recording() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(!state_->attached(), "the world owns its physics system");
    state_->recording = true;
    state_->recorded_steps.clear();
}

std::vector<RecordedStep> World::stop_recording() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    state_->recording = false;
    return std::exchange(state_->recorded_steps, {});
}

std::expected<CharacterId, CharacterError> World::add_character(const RigDesc& rig,
                                                                const Transform& root) {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    const CharacterId id(static_cast<std::uint32_t>(state_->characters.size()));
    // Each character's parts share a collision group of their own, in which parts joined to each
    // other don't collide.
    auto character =
        Character::create(rig, root, *state_->physics_system, state_->part_layer, id.index());
    if (!character.has_value()) {
        return std::unexpected(std::move(character.error()));
    }
    state_->characters.push_back(std::make_unique<Character>(std::move(*character)));
    return id;
}

void World::remove_character(CharacterId character) {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(character.index() < state_->characters.size(),
                  "the character is one of the world's characters");
    std::unique_ptr<Character>& found = state_->characters[character.index()];
    APLOMB_ASSERT(found != nullptr, "the character is one of the world's characters");
    found.reset();
}

void World::push(CharacterId character, PartId part, JPH::Vec3 world_point, JPH::Vec3 impulse) {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    // Reaching the part checks that the character and the part exist.
    std::ignore = this->character(character).part_body(part);
    state_->pushes.push_back(
        {.character = character, .part = part, .world_point = world_point, .impulse = impulse});
}

const Character& World::character(CharacterId character) const {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(character.index() < state_->characters.size(),
                  "the character is one of the world's characters");
    const std::unique_ptr<Character>& found = state_->characters[character.index()];
    APLOMB_ASSERT(found != nullptr, "the character is one of the world's characters");
    return *found;
}

std::uint64_t World::state_hash() const {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    APLOMB_ASSERT(!state_->attached(), "the world owns its physics system");
    StateHasher hasher;
    state_->physics_system->SaveState(hasher);
    return hasher.hash();
}

JPH::PhysicsSystem& World::physics_system() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    return *state_->physics_system;
}

const JPH::PhysicsSystem& World::physics_system() const {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    return *state_->physics_system;
}

} // namespace aplomb
