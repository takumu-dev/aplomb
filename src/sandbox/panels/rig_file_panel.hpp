#pragma once

#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>

#include <filesystem>
#include <optional>
#include <string>

/// The rig file that characters are added from: a path the owner can change, and the rig last
/// loaded from it, or why it couldn't be, with every problem in the file at its line and column.
class RigFilePanel {
  public:
    /// Loads the rig file at a path.
    ///
    /// \param path The rig file's path.
    explicit RigFilePanel(const std::filesystem::path& path);

    /// Draws the rig file's path into the current ImGui window, with a button that loads it, and
    /// the rig it holds or every problem in it.
    void draw();

    /// The rig last loaded, or none if the last load failed.
    [[nodiscard]] const std::optional<aplomb::RigDesc>& rig() const noexcept;

  private:
    void load();

    std::string path_;
    std::optional<aplomb::RigDesc> rig_;
    // Why the last load failed, or none if it didn't.
    std::optional<aplomb::FileError> error_;
};
