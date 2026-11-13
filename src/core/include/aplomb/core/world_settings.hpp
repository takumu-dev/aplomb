/// \file
/// What a world is created from.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <optional>

namespace aplomb {

/// What a world is created from: its step rate, and what the host lends it.
///
/// A world given no physics system owns one of its own. A world given the host's physics system is
/// attached to it, and its characters' parts go on the object layer the host chooses for them.
///
/// The host keeps ownership of what it lends: the job system, the temporary allocator and any
/// physics system, which must outlive the world. A world never starts threads or allocates scratch
/// memory of its own.
struct WorldSettings {
    /// How many fixed steps the world takes for each second it advances.
    float step_rate = 60.0F;

    /// The job system Jolt runs its work on.
    JPH::JobSystem* job_system = nullptr;

    /// The allocator Jolt takes scratch memory from during a step.
    JPH::TempAllocator* temp_allocator = nullptr;

    /// The host's physics system, for a world attached to it, or `nullptr` for a world that owns
    /// one of its own.
    JPH::PhysicsSystem* physics_system = nullptr;

    /// The object layer of the host's physics system that an attached world puts its characters'
    /// parts on. A world that owns its physics system puts them on `aplomb::object_layers::moving`
    /// itself, and is given none.
    std::optional<JPH::ObjectLayer> part_layer = std::nullopt;
};

} // namespace aplomb
