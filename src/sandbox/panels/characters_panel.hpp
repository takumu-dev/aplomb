#pragma once

#include "session.hpp"
#include <aplomb/core/rig_desc.hpp>

#include <optional>
#include <string>

/// The session's characters: a button that adds one from the loaded rig, and a list of them, each
/// with a button that removes it, none of which work while the session records or plays a replay
/// back.
class CharactersPanel {
  public:
    /// Draws the characters into the current ImGui window.
    ///
    /// \param session The session whose characters these are.
    /// \param rig The rig a character is added from, or none while no rig file is loaded.
    void draw(Session& session, const std::optional<aplomb::RigDesc>& rig);

  private:
    // Why the last character couldn't be added, or empty if it was.
    std::string problem_;
};
