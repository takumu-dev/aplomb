#pragma once

#include <aplomb/core/rig_desc.hpp>

/// A rig of three parts in a chain, standing on the ground in the rig's space: a box pelvis, the
/// root part, 0.95 m up; a capsule thigh 0.3 m below it on a swing-twist joint; and a capsule shin
/// 0.4 m below that on a hinge. The thigh and the shin have muscles.
aplomb::RigDesc chain_rig();
