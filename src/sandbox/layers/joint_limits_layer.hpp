#pragma once

#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <span>

/// Draws how far every joint of every character may turn, over the scene, around the joint's
/// anchor and fixed to its parent part, with a line for where the part points now.
///
/// A swing-twist joint shows the cone its part may swing within, with the line of its twist axis
/// now, and an arc for its twist range, with the line of its plane axis now. A hinge shows an arc
/// for its range, with the line of its normal axis now. A line that leaves its cone or arc shows a
/// joint past its limit.
///
/// \param world The world the characters are in.
/// \param characters The characters whose joints to draw.
/// \param draw What to draw with.
void draw_joint_limits_layer(const aplomb::World& world,
                             std::span<const aplomb::CharacterId> characters, DebugDraw& draw);
