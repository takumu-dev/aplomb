#include <aplomb/core/world.hpp>

#include <aplomb/core/assertion.hpp>
#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/log_sink.hpp>
#include <aplomb/core/object_layers.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Core/Factory.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/StateRecorder.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
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

// A push message, as a world holds it until its next step.
struct Push {
    CharacterId character;
    PartId part;
    JPH::Vec3 world_point;
    JPH::Vec3 impulse;
};

} // namespace

// The physics system keeps pointers to the layer filters, so the state never moves once made. The
// characters come after the physics system, so that they leave it before it is destroyed.
struct World::State {
    BroadPhaseLayers broad_phase_layers;
    ObjectVsBroadPhaseLayers object_vs_broad_phase_layers;
    ObjectLayerPairs object_layer_pairs;
    JPH::PhysicsSystem physics_system;
    float step_duration = 0.0F;
    JPH::JobSystem* job_system = nullptr;
    JPH::TempAllocator* temp_allocator = nullptr;
    // Indexed by character id. A removed character leaves its place empty, so that its id is never
    // given to another. Each character lives apart from the table, so that adding another doesn't
    // move it from under a host's reference.
    std::vector<std::unique_ptr<Character>> characters;
    // The pushes sent since the last step, in the order they were sent.
    std::vector<Push> pushes;
};

std::expected<World, WorldError> World::create(const WorldSettings& settings) {
    APLOMB_ASSERT(JPH::Factory::sInstance != nullptr,
                  "Jolt's factory exists before a world is created");
    // RegisterTypes() makes the default physics material, and UnregisterTypes() releases it.
    APLOMB_ASSERT(JPH::PhysicsMaterial::sDefault != nullptr,
                  "Jolt's types are registered before a world is created");
    APLOMB_ASSERT(settings.job_system != nullptr, "a world is given a job system");
    APLOMB_ASSERT(settings.temp_allocator != nullptr, "a world is given a temporary allocator");

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
    state->physics_system.Init(max_bodies, body_mutexes, max_body_pairs, max_contact_constraints,
                               state->broad_phase_layers, state->object_vs_broad_phase_layers,
                               state->object_layer_pairs);
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
    JPH::BodyInterface& body_interface = state_->physics_system.GetBodyInterface();
    for (const Push& push : state_->pushes) {
        if (const std::unique_ptr<Character>& character =
                state_->characters[push.character.index()];
            character != nullptr) {
            body_interface.AddImpulse(character->part_body(push.part), push.impulse,
                                      push.world_point);
        }
    }
    state_->pushes.clear();
    constexpr int collision_steps = 1;
    const JPH::EPhysicsUpdateError errors = state_->physics_system.Update(
        state_->step_duration, collision_steps, state_->temp_allocator, state_->job_system);
    // Jolt carries on without what didn't fit, so some collisions go unhandled for this step.
    if (errors != JPH::EPhysicsUpdateError::None) {
        log(LogSeverity::warning,
            std::format("a step ran out of room in the physics system for {}, and some collisions "
                        "were missed",
                        describe_shortage(errors)));
    }
    for (const std::unique_ptr<Character>& character : state_->characters) {
        if (character != nullptr) {
            character->update_physics_pose(body_interface);
        }
    }
}

std::expected<CharacterId, CharacterError> World::add_character(const RigDesc& rig,
                                                                const Transform& root) {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    const CharacterId id(static_cast<std::uint32_t>(state_->characters.size()));
    // Each character's parts share a collision group of their own, in which parts joined to each
    // other don't collide.
    auto character = Character::create(rig, root, state_->physics_system, id.index());
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
    StateHasher hasher;
    state_->physics_system.SaveState(hasher);
    return hasher.hash();
}

JPH::PhysicsSystem& World::physics_system() {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    return state_->physics_system;
}

const JPH::PhysicsSystem& World::physics_system() const {
    APLOMB_ASSERT(state_ != nullptr, "the world hasn't been moved from");
    return state_->physics_system;
}

} // namespace aplomb
