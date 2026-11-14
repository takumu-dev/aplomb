#pragma once

#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <span>

/// Draws every character's centre of mass over the scene, as a cross with a line down to a cross on
/// the ground below it, which shows whether the character's weight is over its feet.
///
/// The centre of mass is the mean of the parts' positions, each weighted by the part's mass, since
/// each part's own centre of mass is at its origin.
///
/// \param world The world the characters are in.
/// \param characters The characters whose centres of mass to draw.
/// \param draw What to draw with.
void draw_center_of_mass_layer(const aplomb::World& world,
                               std::span<const aplomb::CharacterId> characters, DebugDraw& draw);
