/// \file
/// A character: one active ragdoll in a world.

#pragma once

#include <aplomb/core/character_error.hpp>
#include <aplomb/core/pose.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

namespace aplomb {

class World;

/// One active ragdoll in a world: a Jolt ragdoll made from a rig, with a body for each part and a
/// constraint for each joint. Parts joined to each other never collide.
///
/// A world makes and owns its characters, and a host reaches one through `World::character`.
class Character {
  public:
    /// Moves a character, leaving `other` with no bodies.
    Character(Character&& other) noexcept;

    /// Moves a character into this one, removing what this one held from its physics system.
    Character& operator=(Character&& other) noexcept;

    Character(const Character&) = delete;
    Character& operator=(const Character&) = delete;

    /// Removes the character's bodies and constraints from its physics system, and destroys them.
    ~Character();

    /// The rig the character was made from, muscles included.
    [[nodiscard]] const RigDesc& rig() const noexcept;

    /// The pose the character has after the world's last step, or its bind pose before the first.
    [[nodiscard]] const Pose& physics_pose() const noexcept;

    /// The Jolt body of one of the character's parts.
    ///
    /// \param part A part of the character's rig.
    [[nodiscard]] JPH::BodyID part_body(PartId part) const;

    /// How many joints the character has: one for each part but the root part.
    [[nodiscard]] std::size_t joint_count() const noexcept;

    /// The part that a joint links to its parent part.
    ///
    /// \param joint A joint of the character's rig.
    [[nodiscard]] PartId joint_part(JointId joint) const;

  private:
    friend class World;

    Character(RigDesc rig, std::vector<std::optional<PartId>> parents,
              JPH::Ref<JPH::Ragdoll> ragdoll, std::vector<JPH::BodyID> part_bodies);

    // Checks the rig, then makes the character's ragdoll in its bind pose, with the rig's space
    // placed in the world by `root`, and adds it to the physics system with its parts on `layer`.
    [[nodiscard]] static std::expected<Character, CharacterError>
    create(const RigDesc& rig, const Transform& root, JPH::PhysicsSystem& physics_system,
           JPH::ObjectLayer layer, JPH::CollisionGroup::GroupID collision_group);

    // Reads the physics pose from the parts' bodies.
    void update_physics_pose(const JPH::BodyInterface& body_interface);

    void remove_from_physics_system() noexcept;

    RigDesc rig_;
    std::vector<std::optional<PartId>> parents_;
    std::vector<PartId> joint_parts_;
    JPH::Ref<JPH::Ragdoll> ragdoll_;
    std::vector<JPH::BodyID> part_bodies_;
    Pose physics_pose_;
};

} // namespace aplomb
