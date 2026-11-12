#include <aplomb/scenes/flat_ground.hpp>

#include <aplomb/core/object_layers.hpp>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace aplomb {

namespace {

constexpr float half_width = 100.0F;
constexpr float half_thickness = 1.0F;

// Jolt combines two bodies' friction as the square root of their product, so ground of 1 keeps
// some grip under parts of Jolt's default 0.2.
constexpr float friction = 1.0F;

} // namespace

JPH::BodyID build_flat_ground(JPH::PhysicsSystem& physics_system) {
    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(JPH::Vec3(half_width, half_thickness, half_width)),
        JPH::RVec3(0.0F, -half_thickness, 0.0F), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
        object_layers::scene);
    settings.mFriction = friction;
    return physics_system.GetBodyInterface().CreateAndAddBody(settings,
                                                              JPH::EActivation::DontActivate);
}

} // namespace aplomb
