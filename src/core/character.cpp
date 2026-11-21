#include <aplomb/core/character.hpp>

#include "rig_check.hpp"
#include <aplomb/core/assertion.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/pose.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/rig_problem.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/transform.hpp>
#include <aplomb/core/typed_id.hpp>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace aplomb {

namespace {

JPH::Ref<JPH::Shape> make_shape(const BoxDesc& box) {
    // Jolt rounds a box's edges by its convex radius, which can't exceed the box's smallest half
    // extent.
    const float convex_radius = std::min(JPH::cDefaultConvexRadius, box.half_extents.ReduceMin());
    const JPH::Ref<JPH::Shape> shape = new JPH::BoxShape(box.half_extents, convex_radius);
    return shape;
}

JPH::Ref<JPH::Shape> make_shape(const CapsuleDesc& capsule) {
    const JPH::Ref<JPH::Shape> shape = new JPH::CapsuleShape(capsule.half_height, capsule.radius);
    return shape;
}

JPH::Ref<JPH::Shape> make_shape(const SphereDesc& sphere) {
    const JPH::Ref<JPH::Shape> shape = new JPH::SphereShape(sphere.radius);
    return shape;
}

JPH::Vec3 to_world_point(const Transform& root, JPH::Vec3 point) {
    return root.position + root.rotation * point;
}

// A joint's first axis, in world space.
JPH::Vec3 to_world_axis(const Transform& root, JPH::Vec3 axis) {
    return root.rotation * axis.Normalized();
}

// A joint's second axis, in world space and made exactly perpendicular to its first, since a rig
// gives its axes only to within the check's tolerance.
JPH::Vec3 to_world_axis_perpendicular_to(const Transform& root, JPH::Vec3 axis,
                                         JPH::Vec3 first_axis) {
    const JPH::Vec3 in_world = to_world_axis(root, axis);
    return (in_world - in_world.Dot(first_axis) * first_axis).Normalized();
}

// The settings of a joint's constraint, in world space, with the rig's space placed by `root`.
JPH::Ref<JPH::TwoBodyConstraintSettings> make_constraint(const SwingTwistJointDesc& swing_twist,
                                                         const Transform& root) {
    const JPH::Ref settings = new JPH::SwingTwistConstraintSettings();
    const JPH::Vec3 twist_axis = to_world_axis(root, swing_twist.twist_axis);
    settings->mSpace = JPH::EConstraintSpace::WorldSpace;
    settings->mPosition1 = settings->mPosition2 = to_world_point(root, swing_twist.anchor);
    settings->mTwistAxis1 = settings->mTwistAxis2 = twist_axis;
    settings->mPlaneAxis1 = settings->mPlaneAxis2 =
        to_world_axis_perpendicular_to(root, swing_twist.plane_axis, twist_axis);
    settings->mNormalHalfConeAngle = swing_twist.normal_half_cone;
    settings->mPlaneHalfConeAngle = swing_twist.plane_half_cone;
    settings->mTwistMinAngle = swing_twist.twist_min;
    settings->mTwistMaxAngle = swing_twist.twist_max;
    return settings.GetPtr();
}

JPH::Ref<JPH::TwoBodyConstraintSettings> make_constraint(const HingeJointDesc& hinge,
                                                         const Transform& root) {
    const JPH::Ref settings = new JPH::HingeConstraintSettings();
    const JPH::Vec3 hinge_axis = to_world_axis(root, hinge.hinge_axis);
    settings->mSpace = JPH::EConstraintSpace::WorldSpace;
    settings->mPoint1 = settings->mPoint2 = to_world_point(root, hinge.anchor);
    settings->mHingeAxis1 = settings->mHingeAxis2 = hinge_axis;
    settings->mNormalAxis1 = settings->mNormalAxis2 =
        to_world_axis_perpendicular_to(root, hinge.normal_axis, hinge_axis);
    settings->mLimitsMin = hinge.limit_min;
    settings->mLimitsMax = hinge.limit_max;
    return settings.GetPtr();
}

// The ragdoll's settings, with every part on `layer`. Jolt wants each part after its parent part,
// so the ragdoll's parts are in the order of `parents_first` rather than the rig's.
JPH::Ref<JPH::RagdollSettings> make_ragdoll_settings(const RigDesc& rig,
                                                     const detail::RigStructure& structure,
                                                     const Transform& root,
                                                     JPH::ObjectLayer layer) {
    const JPH::Ref skeleton = new JPH::Skeleton();
    std::vector<int> ragdoll_index(rig.parts.size());
    for (std::size_t index = 0; index < structure.parents_first.size(); ++index) {
        const PartId part = structure.parents_first[index];
        const std::optional<PartId> parent = structure.parents[part.index()];
        ragdoll_index[part.index()] = static_cast<int>(index);
        skeleton->AddJoint(rig.parts[part.index()].name,
                           parent.has_value() ? ragdoll_index[parent->index()] : -1);
    }

    const JPH::Ref settings = new JPH::RagdollSettings();
    settings->mSkeleton = skeleton;
    settings->mParts.resize(rig.parts.size());
    for (std::size_t index = 0; index < structure.parents_first.size(); ++index) {
        const PartDesc& part = rig.parts[structure.parents_first[index].index()];
        JPH::RagdollSettings::Part& ragdoll_part = settings->mParts[index];
        ragdoll_part.SetShape(
            std::visit([](const auto& shape) { return make_shape(shape); }, part.shape));
        ragdoll_part.mPosition = to_world_point(root, part.bind_position);
        ragdoll_part.mRotation = (root.rotation * part.bind_rotation.Normalized()).Normalized();
        ragdoll_part.mMotionType = JPH::EMotionType::Dynamic;
        ragdoll_part.mObjectLayer = layer;
        ragdoll_part.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        ragdoll_part.mMassPropertiesOverride.mMass = part.mass;
        ragdoll_part.mFriction = part.friction;
        if (part.joint.has_value()) {
            ragdoll_part.mToParent = std::visit(
                [&](const auto& joint) { return make_constraint(joint, root); }, *part.joint);
        }
    }
    // Solving the joints nearest the root part last lets them win, which keeps a long chain of
    // parts from stretching.
    settings->CalculateConstraintPriorities();
    settings->DisableParentChildCollisions();
    return settings;
}

CharacterError invalid_rig(const RigDesc& rig, std::vector<RigProblem> problems) {
    std::string message = std::format("a character can't be made from the rig \"{}\": ", rig.name);
    for (std::size_t index = 0; index < problems.size(); ++index) {
        message += index == 0 ? "" : "; ";
        message += problems[index].message;
    }
    return {
        .code = CharacterErrorCode::invalid_rig,
        .message = std::move(message),
        .problems = std::move(problems),
    };
}

} // namespace

Character::Character(RigDesc rig, std::vector<std::optional<PartId>> parents,
                     JPH::Ref<JPH::Ragdoll> ragdoll, std::vector<JPH::BodyID> part_bodies)
    : rig_(std::move(rig)), parents_(std::move(parents)), ragdoll_(std::move(ragdoll)),
      part_bodies_(std::move(part_bodies)) {
    for (std::size_t part = 0; part < parents_.size(); ++part) {
        if (parents_[part].has_value()) {
            joint_parts_.emplace_back(static_cast<std::uint32_t>(part));
        }
    }
}

std::expected<Character, CharacterError>
Character::create(const RigDesc& rig, const Transform& root, JPH::PhysicsSystem& physics_system,
                  JPH::ObjectLayer layer, JPH::CollisionGroup::GroupID collision_group) {
    APLOMB_ASSERT(root.rotation.IsNormalized(), "a character's root rotation is a unit quaternion");
    auto structure = detail::check_rig(rig);
    if (!structure.has_value()) {
        return std::unexpected(invalid_rig(rig, std::move(structure.error())));
    }

    const JPH::Ref<JPH::RagdollSettings> settings =
        make_ragdoll_settings(rig, *structure, root, layer);
    // Jolt destroys whatever bodies it made of a ragdoll that didn't fit.
    JPH::Ref<JPH::Ragdoll> ragdoll = settings->CreateRagdoll(collision_group, 0, &physics_system);
    if (ragdoll == nullptr) {
        return std::unexpected(CharacterError{
            .code = CharacterErrorCode::out_of_bodies,
            .message = std::format("the physics system has no room for the {} bodies of a "
                                   "character made from the rig \"{}\"",
                                   rig.parts.size(), rig.name),
            .problems = {},
        });
    }
    ragdoll->AddToPhysicsSystem(JPH::EActivation::Activate);

    std::vector<JPH::BodyID> part_bodies(rig.parts.size());
    for (std::size_t index = 0; index < structure->parents_first.size(); ++index) {
        part_bodies[structure->parents_first[index].index()] =
            ragdoll->GetBodyID(static_cast<int>(index));
    }

    Character character(rig, std::move(structure->parents), std::move(ragdoll),
                        std::move(part_bodies));
    character.update_physics_pose(physics_system.GetBodyInterface());
    return character;
}

Character::Character(Character&& other) noexcept = default;

Character& Character::operator=(Character&& other) noexcept {
    if (this != &other) {
        remove_from_physics_system();
        rig_ = std::move(other.rig_);
        parents_ = std::move(other.parents_);
        joint_parts_ = std::move(other.joint_parts_);
        ragdoll_ = std::move(other.ragdoll_);
        part_bodies_ = std::move(other.part_bodies_);
        physics_pose_ = std::move(other.physics_pose_);
    }
    return *this;
}

Character::~Character() { remove_from_physics_system(); }

const RigDesc& Character::rig() const noexcept { return rig_; }

const Pose& Character::physics_pose() const noexcept { return physics_pose_; }

JPH::BodyID Character::part_body(PartId part) const {
    APLOMB_ASSERT(part.index() < part_bodies_.size(), "the part is one of the character's parts");
    return part_bodies_[part.index()];
}

std::size_t Character::joint_count() const noexcept { return joint_parts_.size(); }

PartId Character::joint_part(JointId joint) const {
    APLOMB_ASSERT(joint.index() < joint_parts_.size(),
                  "the joint is one of the character's joints");
    return joint_parts_[joint.index()];
}

void Character::update_physics_pose(const JPH::BodyInterface& body_interface) {
    std::vector<Transform> in_world(part_bodies_.size());
    for (std::size_t part = 0; part < part_bodies_.size(); ++part) {
        body_interface.GetPositionAndRotation(part_bodies_[part], in_world[part].position,
                                              in_world[part].rotation);
    }

    std::vector<Transform> relative = in_world;
    for (std::size_t part = 0; part < parents_.size(); ++part) {
        if (const std::optional<PartId> parent = parents_[part]; parent.has_value()) {
            const Transform& parent_in_world = in_world[parent->index()];
            const JPH::Quat to_parent = parent_in_world.rotation.Conjugated();
            relative[part] = {
                .position = to_parent * (in_world[part].position - parent_in_world.position),
                .rotation = (to_parent * in_world[part].rotation).Normalized(),
            };
        }
    }
    physics_pose_ = Pose(std::move(relative));
}

void Character::remove_from_physics_system() noexcept {
    // A moved-from character has no ragdoll. Destroying the ragdoll destroys its bodies, which
    // must leave the physics system first.
    if (ragdoll_ != nullptr) {
        ragdoll_->RemoveFromPhysicsSystem();
        ragdoll_ = nullptr;
    }
}

} // namespace aplomb
