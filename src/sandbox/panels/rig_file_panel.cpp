#include "panels/rig_file_panel.hpp"

#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/rig_file.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>

RigFilePanel::RigFilePanel(const std::filesystem::path& path) : path_(path.string()) { load(); }

void RigFilePanel::draw() {
    // The path takes the row, bar room for the button beside it.
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(
        -(ImGui::CalcTextSize("Load").x + (2.0F * style.FramePadding.x) + style.ItemSpacing.x));
    ImGui::InputText("##rig_file", &path_);
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        load();
    }
    if (rig_) {
        ImGui::TextUnformatted(std::format("{}, {} parts", rig_->name, rig_->parts.size()).c_str());
    } else {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextUnformatted(problem_.c_str());
        ImGui::PopTextWrapPos();
    }
}

const std::optional<aplomb::RigDesc>& RigFilePanel::rig() const noexcept { return rig_; }

void RigFilePanel::load() {
    std::expected<aplomb::RigDesc, aplomb::FileError> rig =
        aplomb::load_rig_file(std::filesystem::path(path_));
    if (rig) {
        rig_ = std::move(*rig);
        problem_.clear();
    } else {
        rig_.reset();
        problem_ = std::format("{}", rig.error());
    }
}
