#pragma once

#include "layers/debug_layers.hpp"

/// Draws a checkbox for each debug layer into the current ImGui window, which turns it on and off.
///
/// \param layers The layers the checkboxes turn on and off.
void draw_layers_panel(DebugLayers& layers);
