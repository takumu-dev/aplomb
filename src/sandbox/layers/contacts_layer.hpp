#pragma once

#include "contact_tracker.hpp"
#include "debug_draw/debug_draw.hpp"

#include <span>

/// Draws every contact over the scene: a cross where the bodies touch, and a line along the normal
/// that pushes the second body out of the first.
///
/// \param contacts The contacts to draw.
/// \param draw What to draw with.
void draw_contacts_layer(std::span<const Contact> contacts, DebugDraw& draw);
