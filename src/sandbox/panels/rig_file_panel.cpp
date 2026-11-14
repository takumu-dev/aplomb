#include "panels/rig_file_panel.hpp"

#include "panels/imgui_flags.hpp"
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>
#include <aplomb/files/rig_file.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace {

// The most problems the list shows in full; a longer list scrolls, about this many lines tall.
constexpr std::size_t listed_problems = 8;

// Draws every problem in a file, each with its line and column.
void draw_problems(const aplomb::FileError& error) {
    const std::size_t count = error.problems.size();
    ImGui::TextUnformatted(
        std::format("The rig file has {} problem{}:", count, count == 1 ? "" : "s").c_str());
    ImGuiTableFlags flags =
        imgui_flags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg, ImGuiTableFlags_SizingFixedFit);
    float height = 0.0F;
    if (count > listed_problems) {
        flags = imgui_flags(flags, ImGuiTableFlags_ScrollY);
        const float row_height =
            ImGui::GetTextLineHeight() + (2.0F * ImGui::GetStyle().CellPadding.y);
        height = row_height * static_cast<float>(listed_problems + 1);
    }
    if (!ImGui::BeginTable("problems", 3, flags, {0.0F, height})) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Column");
    ImGui::TableSetupColumn("Problem", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (const aplomb::FileProblem& problem : error.problems) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::format("{}", problem.line).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::format("{}", problem.column).c_str());
        ImGui::TableNextColumn();
        const std::string text = problem.pointer.empty()
                                     ? problem.message
                                     : std::format("{}: {}", problem.pointer, problem.message);
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndTable();
}

} // namespace

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
    } else if (error_ && !error_->problems.empty()) {
        draw_problems(*error_);
    } else if (error_) {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextUnformatted(error_->message.c_str());
        ImGui::PopTextWrapPos();
    }
}

const std::optional<aplomb::RigDesc>& RigFilePanel::rig() const noexcept { return rig_; }

void RigFilePanel::load() {
    std::expected<aplomb::RigDesc, aplomb::FileError> rig =
        aplomb::load_rig_file(std::filesystem::path(path_));
    if (rig) {
        rig_ = std::move(*rig);
        error_.reset();
    } else {
        rig_.reset();
        error_ = std::move(rig.error());
    }
}
