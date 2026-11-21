#include "host_physics.hpp"

#include "host_services.hpp"
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace {

constexpr JPH::uint max_bodies = 1024;
constexpr JPH::uint body_mutexes = 0; // Jolt's default
constexpr JPH::uint max_body_pairs = 65536;
constexpr JPH::uint max_contact_constraints = 10240;

constexpr JPH::BroadPhaseLayer still(0);
constexpr JPH::BroadPhaseLayer moving(1);

constexpr float ground_half_width = 50.0F;
constexpr float ground_half_thickness = 0.5F;

} // namespace

JPH::uint HostPhysics::BroadPhaseLayers::GetNumBroadPhaseLayers() const { return 2; }

JPH::BroadPhaseLayer
HostPhysics::BroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
    return layer == host_layers::scenery ? still : moving;
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char*
HostPhysics::BroadPhaseLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const {
    return layer == still ? "still" : "moving";
}
#endif

bool HostPhysics::ObjectVsBroadPhaseLayers::ShouldCollide(
    JPH::ObjectLayer object_layer, JPH::BroadPhaseLayer broad_phase_layer) const {
    return object_layer != host_layers::scenery || broad_phase_layer != still;
}

bool HostPhysics::ObjectLayerPairs::ShouldCollide(JPH::ObjectLayer first,
                                                  JPH::ObjectLayer second) const {
    return first != host_layers::scenery || second != host_layers::scenery;
}

HostPhysics::HostPhysics() {
    physics_system_.Init(max_bodies, body_mutexes, max_body_pairs, max_contact_constraints,
                         broad_phase_layers_, object_vs_broad_phase_layers_, object_layer_pairs_);
}

aplomb::WorldSettings HostPhysics::world_settings(HostServices& services) noexcept {
    aplomb::WorldSettings settings = services.world_settings();
    settings.physics_system = &physics_system_;
    settings.part_layer = host_layers::characters;
    return settings;
}

JPH::BodyID HostPhysics::add_ground() {
    const JPH::BodyCreationSettings settings(
        new JPH::BoxShape(JPH::Vec3(ground_half_width, ground_half_thickness, ground_half_width)),
        JPH::RVec3(0.0F, -ground_half_thickness, 0.0F), JPH::Quat::sIdentity(),
        JPH::EMotionType::Static, host_layers::scenery);
    return physics_system_.GetBodyInterface().CreateAndAddBody(settings,
                                                               JPH::EActivation::DontActivate);
}

void HostPhysics::update(HostServices& services, float duration, int collision_steps) {
    physics_system_.Update(duration, collision_steps, &services.temp_allocator(),
                           &services.job_system());
}
