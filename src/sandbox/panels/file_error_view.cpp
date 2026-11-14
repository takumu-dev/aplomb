#include "panels/file_error_view.hpp"

#include "panels/imgui_flags.hpp"
#include <aplomb/files/file_error.hpp>
#include <aplomb/files/file_problem.hpp>

#include <imgui.h>

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace {

// The most problems the list shows in full; a longer list scrolls, about this many lines tall.
constexpr std::size_t listed_problems = 8;

// Draws every problem in a file, each with its line and column.
void draw_problems(const aplomb::FileError& error, std::string_view kind) {
    const std::size_t count = error.problems.size();
    ImGui::TextUnformatted(
        std::format("The {} has {} problem{}:", kind, count, count == 1 ? "" : "s").c_str());
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

void draw_file_error(const aplomb::FileError& error, std::string_view kind) {
    if (!error.problems.empty()) {
        // Each kind of file's table is its own, should a window show two.
        ImGui::PushID(std::string(kind).c_str());
        draw_problems(error, kind);
        ImGui::PopID();
        return;
    }
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(error.message.c_str());
    ImGui::PopTextWrapPos();
}
