#pragma once

#include "debug_draw/debug_draw.hpp"
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>

#include <span>

/// Draws the ground, as a grid of lines on its top face, and every part of every character, each
/// character in a colour of its own that it keeps while others come and go.
///
/// \param world The world the characters are in.
/// \param characters The characters to draw.
/// \param draw What to draw with.
void draw_shapes_layer(const aplomb::World& world, std::span<const aplomb::CharacterId> characters,
                       DebugDraw& draw);
