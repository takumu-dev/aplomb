#pragma once

#include "panels/rig_file_panel.hpp"
#include "session.hpp"
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/replay_desc.hpp>

#include <optional>
#include <string>

/// Records the session as a replay and saves it, or loads a replay and plays it back.
///
/// Recording starts the session afresh, with as many characters as it had, all made from the loaded
/// rig, and stopping keeps the replay to be saved to the replay file's path. Playing a replay back
/// loads it from that path and starts a session that plays it back, with characters made from the
/// loaded rig, which must be read from the rig file the replay was recorded with.
class ReplayPanel {
  public:
    /// Draws the replay file's path into the current ImGui window, with buttons that record, save
    /// and play, and how the replay played back.
    ///
    /// \param session The session to record, or to replace with one that records or plays back.
    /// \param rig_file The rig file whose rig characters are made from.
    /// \return Whether the session was replaced.
    [[nodiscard]] bool draw(Session& session, const RigFilePanel& rig_file);

  private:
    std::string path_ = "replay.json";
    // The replay last recorded, until another is.
    std::optional<aplomb::ReplayDesc> recorded_;
    // What became of the last thing tried, such as where a replay was saved, or why it couldn't
    // be.
    std::string status_;
    // Why the last replay file couldn't be read, or none if it could.
    std::optional<aplomb::FileError> error_;
};
