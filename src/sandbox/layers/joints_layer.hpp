#pragma once

#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <span>

/// Draws every joint of every character over the scene: a cross at its anchor, in a colour for its
/// kind, joined by lines to the origins of its part and its parent part.
///
/// The anchor is carried along with each of the two parts, so that a joint that has come apart
/// shows as a gap between the lines.
///
/// \param world The world the characters are in.
/// \param characters The characters whose joints to draw.
/// \param draw What to draw with.
void draw_joints_layer(const aplomb::World& world, std::span<const aplomb::CharacterId> characters,
                       DebugDraw& draw);
