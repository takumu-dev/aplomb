#include "rigs.hpp"

#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/shape_desc.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>

aplomb::RigDesc chain_rig() {
    return {
        .name = "chain",
        .parts =
            {
                {
                    .name = "pelvis",
                    .shape = aplomb::BoxDesc{.half_extents = JPH::Vec3(0.15F, 0.1F, 0.1F)},
                    .mass = 10.0F,
                    .bind_position = JPH::Vec3(0.0F, 0.95F, 0.0F),
                },
                {
                    .name = "thigh",
                    .parent = "pelvis",
                    .shape = aplomb::CapsuleDesc{.radius = 0.07F, .half_height = 0.15F},
                    .mass = 8.0F,
                    .bind_position = JPH::Vec3(0.0F, 0.65F, 0.0F),
                    .joint =
                        aplomb::SwingTwistJointDesc{
                            .anchor = JPH::Vec3(0.0F, 0.85F, 0.0F),
                            .twist_axis = JPH::Vec3(0.0F, -1.0F, 0.0F),
                            .plane_axis = JPH::Vec3(1.0F, 0.0F, 0.0F),
                            .normal_half_cone = 1.0F,
                            .plane_half_cone = 0.8F,
                            .twist_min = -0.3F,
                            .twist_max = 0.3F,
                        },
                    .muscle =
                        aplomb::MuscleDesc{
                            .stiffness = 400.0F,
                            .damping = 40.0F,
                            .max_torque = 250.0F,
                        },
                },
                {
                    .name = "shin",
                    .parent = "thigh",
                    .shape = aplomb::CapsuleDesc{.radius = 0.06F, .half_height = 0.15F},
                    .mass = 4.0F,
                    .bind_position = JPH::Vec3(0.0F, 0.25F, 0.0F),
                    .joint =
                        aplomb::HingeJointDesc{
                            .anchor = JPH::Vec3(0.0F, 0.45F, 0.0F),
                            .hinge_axis = JPH::Vec3(1.0F, 0.0F, 0.0F),
                            .normal_axis = JPH::Vec3(0.0F, -1.0F, 0.0F),
                            .limit_min = -2.4F,
                            .limit_max = 0.0F,
                        },
                    .muscle =
                        aplomb::MuscleDesc{
                            .stiffness = 200.0F,
                            .damping = 20.0F,
                            .max_torque = 120.0F,
                        },
                },
            },
    };
}
