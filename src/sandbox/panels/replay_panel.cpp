#include "panels/replay_panel.hpp"

#include "panels/file_error_view.hpp"
#include "panels/rig_file_panel.hpp"
#include "playback.hpp"
#include "session.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/replay_desc.hpp>
#include <aplomb/files/replay_file.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace {

// Draws how far the session's replay has been played back, and whether its state hashes were the
// replay's.
void draw_playback(const Session& session) {
    const std::optional<Playback>& playback = session.playback();
    if (!playback) {
        return;
    }
    if (!playback->finished()) {
        ImGui::TextUnformatted(
            std::format("Playing back, {} of {} steps", playback->played, playback->steps).c_str());
    } else if (!playback->first_differing_step) {
        ImGui::TextUnformatted(
            std::format("Played back {} steps, every state hash the replay's", playback->steps)
                .c_str());
    }
    if (playback->first_differing_step) {
        const auto& [step, hashes] = *playback->first_differing_step;
        ImGui::TextUnformatted(std::format("The state hash differed first at step {}: the replay "
                                           "has {:016x}, the session {:016x}",
                                           step, hashes[0], hashes[1])
                                   .c_str());
    }
}

} // namespace

bool ReplayPanel::draw(Session& session, const RigFilePanel& rig_file) {
    const std::optional<aplomb::RigDesc>& rig = rig_file.rig();
    const std::optional<std::uint64_t> rig_file_hash = rig_file.rig_file_hash();
    bool replaced = false;

    if (session.recording()) {
        if (ImGui::Button("Stop recording")) {
            recorded_ = session.stop_recording();
            status_ = std::format("Recorded {} steps", recorded_->steps.size());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(std::format("Recording, {} steps", session.steps_taken()).c_str());
    } else {
        ImGui::BeginDisabled(!rig.has_value());
        if (ImGui::Button("Record") && rig && rig_file_hash) {
            std::expected<Session, std::string> recorded = Session::record(
                session.settings(), *rig, *rig_file_hash, session.characters().size());
            if (recorded) {
                session = std::move(*recorded);
                replaced = true;
                status_.clear();
            } else {
                status_ = std::format("The session couldn't be recorded: {}", recorded.error());
            }
            error_.reset();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("starts afresh from the loaded rig");
        ImGui::EndDisabled();
    }

    // The path takes the row, bar room for the buttons beside it.
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(-(ImGui::CalcTextSize("SavePlay").x + (4.0F * style.FramePadding.x) +
                              (2.0F * style.ItemSpacing.x)));
    ImGui::InputText("##replay_file", &path_);
    ImGui::SameLine();
    ImGui::BeginDisabled(!recorded_.has_value());
    if (ImGui::Button("Save") && recorded_) {
        const std::expected<void, aplomb::FileError> saved =
            aplomb::save_replay_file(std::filesystem::path(path_), *recorded_);
        status_ = saved ? std::format("Saved {} steps to {}", recorded_->steps.size(), path_)
                        : std::format("{}", saved.error());
        error_.reset();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // Playing a replay back would lose what is being recorded.
    ImGui::BeginDisabled(session.recording() || !rig.has_value());
    if (ImGui::Button("Play") && rig && rig_file_hash) {
        std::expected<aplomb::ReplayDesc, aplomb::FileError> replay =
            aplomb::load_replay_file(std::filesystem::path(path_));
        status_.clear();
        error_.reset();
        if (!replay) {
            error_ = std::move(replay.error());
        } else if (std::expected<Session, std::string> playing = Session::play(
                       session.settings(), std::move(*replay), *rig, *rig_file_hash)) {
            session = std::move(*playing);
            replaced = true;
        } else {
            status_ = std::format("The replay can't be played back: {}", playing.error());
        }
    }
    ImGui::EndDisabled();

    ImGui::PushTextWrapPos(0.0F);
    if (!status_.empty()) {
        ImGui::TextUnformatted(status_.c_str());
    }
    if (error_) {
        draw_file_error(*error_, "replay file");
    }
    draw_playback(session);
    ImGui::PopTextWrapPos();
    return replaced;
}
