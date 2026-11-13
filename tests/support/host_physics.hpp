#pragma once

#include "host_services.hpp"
#include <aplomb/core/world_settings.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

/// The object layers of a host's own physics system, numbered unlike aplomb's own.
namespace host_layers {

/// The static bodies of the host's scene.
inline constexpr JPH::ObjectLayer scenery = 0;

/// The host's own bodies that move.
inline constexpr JPH::ObjectLayer props = 1;

/// The parts of the characters of a world attached to the host's physics system.
inline constexpr JPH::ObjectLayer characters = 2;

} // namespace host_layers

/// A physics system that a host makes and steps itself, to which a world attaches. Its scenery
/// collides with everything else, and props and characters collide with each other too.
class HostPhysics {
  public:
    HostPhysics();
    HostPhysics(const HostPhysics&) = delete;
    HostPhysics(HostPhysics&&) = delete;
    HostPhysics& operator=(const HostPhysics&) = delete;
    HostPhysics& operator=(HostPhysics&&) = delete;
    ~HostPhysics() = default;

    /// The physics system.
    JPH::PhysicsSystem& physics_system() noexcept { return physics_system_; }

    /// Settings for a world at the default step rate, attached to this physics system with its
    /// characters' parts on `host_layers::characters`, lent the host's job system and allocator.
    aplomb::WorldSettings world_settings(HostServices& services) noexcept;

    /// Adds a static box to the scenery whose top face is level at a height of 0, and returns its
    /// body.
    JPH::BodyID add_ground();

    /// Steps the physics system once by `duration` seconds, split into `collision_steps`, on the
    /// host's job system and temporary allocator.
    void update(HostServices& services, float duration, int collision_steps = 1);

  private:
    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
      public:
        [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;
        [[nodiscard]] JPH::BroadPhaseLayer
        GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
    };

    class ObjectVsBroadPhaseLayers final : public JPH::ObjectVsBroadPhaseLayerFilter {
      public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object_layer,
                                         JPH::BroadPhaseLayer broad_phase_layer) const override;
    };

    class ObjectLayerPairs final : public JPH::ObjectLayerPairFilter {
      public:
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
                                         JPH::ObjectLayer second) const override;
    };

    BroadPhaseLayers broad_phase_layers_;
    ObjectVsBroadPhaseLayers object_vs_broad_phase_layers_;
    ObjectLayerPairs object_layer_pairs_;
    JPH::PhysicsSystem physics_system_;
};
