#include "panels/rig_file_panel.hpp"

#include "panels/file_error_view.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/rig_file.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
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
    } else if (error_) {
        draw_file_error(*error_, "rig file");
    }
}

const std::optional<aplomb::RigDesc>& RigFilePanel::rig() const noexcept { return rig_; }

std::optional<std::uint64_t> RigFilePanel::rig_file_hash() const noexcept { return hash_; }

void RigFilePanel::load() {
    const std::filesystem::path path(path_);
    std::expected<aplomb::RigDesc, aplomb::FileError> rig = aplomb::load_rig_file(path);
    std::expected<std::uint64_t, aplomb::FileError> hash =
        rig ? aplomb::load_rig_file_hash(path) : std::unexpected(std::move(rig.error()));
    if (hash) {
        rig_ = std::move(*rig);
        hash_ = *hash;
        error_.reset();
    } else {
        rig_.reset();
        hash_.reset();
        error_ = std::move(hash.error());
    }
}
