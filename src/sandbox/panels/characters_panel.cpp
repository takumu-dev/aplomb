#include "panels/characters_panel.hpp"

#include "session.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/character_error.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/typed_id.hpp>

#include <imgui.h>

#include <expected>
#include <format>
#include <optional>

void CharactersPanel::draw(Session& session, const std::optional<aplomb::RigDesc>& rig) {
    ImGui::BeginDisabled(!rig.has_value());
    if (ImGui::Button("Add character") && rig) {
        const std::expected<aplomb::CharacterId, aplomb::CharacterError> added =
            session.add_character(*rig);
        problem_ = added ? "" : std::format("{}", added.error());
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(session.characters().empty());
    const bool remove_all = ImGui::Button("Remove all");
    ImGui::EndDisabled();
    if (!problem_.empty()) {
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextUnformatted(problem_.c_str());
        ImGui::PopTextWrapPos();
    }

    // Characters are removed once the list is drawn, since removing one changes the list.
    std::optional<aplomb::CharacterId> removed;
    for (const aplomb::CharacterId character : session.characters()) {
        ImGui::PushID(static_cast<int>(character.index()));
        if (ImGui::SmallButton("Remove")) {
            removed = character;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(std::format("Character {}, {}", character.index(),
                                           session.world().character(character).rig().name)
                                   .c_str());
        ImGui::PopID();
    }
    if (remove_all) {
        while (!session.characters().empty()) {
            session.remove_character(session.characters().back());
        }
    } else if (removed) {
        session.remove_character(*removed);
    }
}
