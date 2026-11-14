#pragma once

#include "push_tool.hpp"

/// Draws the push tool's control into the current ImGui window, the impulse each metre of drag
/// gives, with how to push.
///
/// \param tool The push tool the control changes.
void draw_push_panel(PushTool& tool);
