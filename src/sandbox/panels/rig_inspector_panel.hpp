#pragma once

#include "session.hpp"
#include <aplomb/core/typed_id.hpp>

#include <optional>

/// A window that lists one of the session's characters' parts and joints with their settings, as
/// its rig gives them, the character chosen from a list of them all.
class RigInspectorPanel {
  public:
    /// Draws the window.
    ///
    /// \param session The session whose characters can be inspected.
    void draw(const Session& session);

  private:
    // The character inspected, which falls back to the first when it is removed.
    std::optional<aplomb::CharacterId> inspected_;
};
