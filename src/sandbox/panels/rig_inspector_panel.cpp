#include "panels/rig_inspector_panel.hpp"

#include "panels/imgui_flags.hpp"
#include "session.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/muscle_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/typed_id.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace {

constexpr float window_margin = 10.0F;
constexpr float window_width = 560.0F;
constexpr float window_height = 600.0F;

// A rotation smaller than this, in radians, is shown as none.
constexpr float least_rotation = 1.0e-6F;

constexpr ImGuiTableFlags table_flags =
    imgui_flags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg, ImGuiTableFlags_SizingFixedFit,
                ImGuiTableFlags_ScrollX);

// Begins a table whose columns fit their contents, which scrolls sideways when they are wider than
// the window, and is tall enough for all its rows and its header.
bool begin_table(const char* id, int columns, std::size_t rows) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float row_height = ImGui::GetTextLineHeight() + (2.0F * style.CellPadding.y);
    const float height = (row_height * static_cast<float>(rows + 1)) + style.ScrollbarSize +
                         (2.0F * style.CellPadding.y);
    return ImGui::BeginTable(id, columns, table_flags, {0.0F, height});
}

std::string character_label(const Session& session, aplomb::CharacterId character) {
    return std::format("Character {}, {}", character.index(),
                       session.world().character(character).rig().name);
}

std::string point(JPH::Vec3 vector) {
    return std::format("({:.3f}, {:.3f}, {:.3f})", vector.GetX(), vector.GetY(), vector.GetZ());
}

std::string axis(JPH::Vec3 vector) {
    return std::format("({:.2f}, {:.2f}, {:.2f})", vector.GetX(), vector.GetY(), vector.GetZ());
}

float degrees(float radians) { return JPH::RadiansToDegrees(radians); }

std::string rotation(JPH::Quat quaternion) {
    JPH::Vec3 about = JPH::Vec3::sZero();
    float angle = 0.0F;
    // Of the two quaternions for a rotation, the one with a positive W turns the shorter way.
    (quaternion.GetW() < 0.0F ? -quaternion : quaternion).GetAxisAngle(about, angle);
    if (angle < least_rotation) {
        return "none";
    }
    return std::format("{:.1f} deg about {}", degrees(angle), axis(about));
}

std::string shape(const aplomb::BoxDesc& box) {
    return std::format("box, half extents {}", point(box.half_extents));
}

std::string shape(const aplomb::CapsuleDesc& capsule) {
    return std::format("capsule, radius {:.3f}, half height {:.3f}", capsule.radius,
                       capsule.half_height);
}

std::string shape(const aplomb::SphereDesc& sphere) {
    return std::format("sphere, radius {:.3f}", sphere.radius);
}

std::string_view kind(const aplomb::SwingTwistJointDesc& /*joint*/) { return "swing-twist"; }

std::string_view kind(const aplomb::HingeJointDesc& /*joint*/) { return "hinge"; }

std::string axes(const aplomb::SwingTwistJointDesc& joint) {
    return std::format("twist {}, plane {}", axis(joint.twist_axis), axis(joint.plane_axis));
}

std::string axes(const aplomb::HingeJointDesc& joint) {
    return std::format("hinge {}, normal {}", axis(joint.hinge_axis), axis(joint.normal_axis));
}

std::string limits(const aplomb::SwingTwistJointDesc& joint) {
    return std::format("normal cone {:.1f}, plane cone {:.1f}, twist {:.1f} to {:.1f}",
                       degrees(joint.normal_half_cone), degrees(joint.plane_half_cone),
                       degrees(joint.twist_min), degrees(joint.twist_max));
}

std::string limits(const aplomb::HingeJointDesc& joint) {
    return std::format("{:.1f} to {:.1f}", degrees(joint.limit_min), degrees(joint.limit_max));
}

void header(std::initializer_list<const char*> columns) {
    for (const char* column : columns) {
        ImGui::TableSetupColumn(column);
    }
    ImGui::TableHeadersRow();
}

void row(std::initializer_list<std::string_view> cells) {
    ImGui::TableNextRow();
    for (const std::string_view cell : cells) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::string(cell).c_str());
    }
}

void draw_parts(const aplomb::RigDesc& rig) {
    if (!begin_table("parts", 7, rig.parts.size())) {
        return;
    }
    header({
        "Part",
        "Parent",
        "Shape",
        "Mass (kg)",
        "Friction",
        "Bind position (m)",
        "Bind rotation",
    });
    for (const aplomb::PartDesc& part : rig.parts) {
        row({
            part.name,
            part.parent.value_or("none (root part)"),
            std::visit([](const auto& kind) { return shape(kind); }, part.shape),
            std::format("{:.3f}", part.mass),
            std::format("{:.2f}", part.friction),
            point(part.bind_position),
            rotation(part.bind_rotation),
        });
    }
    ImGui::EndTable();
}

void draw_joints(const aplomb::Character& character) {
    if (!begin_table("joints", 10, character.joint_count())) {
        return;
    }
    header({
        "Joint",
        "Part",
        "Parent",
        "Kind",
        "Anchor (m)",
        "Axes",
        "Limits (deg)",
        "Stiffness (N m/rad)",
        "Damping (N m s/rad)",
        "Max torque (N m)",
    });
    const std::span<const aplomb::PartDesc> parts = character.rig().parts;
    for (std::size_t joint = 0; joint < character.joint_count(); ++joint) {
        const aplomb::PartDesc& part =
            parts[character.joint_part(aplomb::JointId(static_cast<std::uint32_t>(joint))).index()];
        // Every part a joint links has a parent part, a joint and a muscle.
        if (!part.parent || !part.joint || !part.muscle) {
            continue;
        }
        const aplomb::JointDesc& desc = *part.joint;
        const aplomb::MuscleDesc& muscle = *part.muscle;
        const auto visit = [&](const auto& describe) { return std::visit(describe, desc); };
        row({
            std::format("{}", joint),
            part.name,
            *part.parent,
            visit([](const auto& kind_of) { return kind(kind_of); }),
            point(visit([](const auto& kind_of) { return kind_of.anchor; })),
            visit([](const auto& kind_of) { return axes(kind_of); }),
            visit([](const auto& kind_of) { return limits(kind_of); }),
            std::format("{:g}", muscle.stiffness),
            std::format("{:g}", muscle.damping),
            std::format("{:g}", muscle.max_torque),
        });
    }
    ImGui::EndTable();
}

// Draws a list to choose the character inspected from.
//
// Returns the character chosen, or the one inspected if none is.
aplomb::CharacterId choose_character(const Session& session, aplomb::CharacterId inspected) {
    if (ImGui::BeginCombo("Character", character_label(session, inspected).c_str())) {
        for (const aplomb::CharacterId character : session.characters()) {
            if (ImGui::Selectable(character_label(session, character).c_str(),
                                  character == inspected)) {
                inspected = character;
            }
        }
        ImGui::EndCombo();
    }
    return inspected;
}

// Draws a character's rig: its parts, and its joints.
void draw_rig(const aplomb::Character& character) {
    const aplomb::RigDesc& rig = character.rig();
    ImGui::TextUnformatted(
        std::format("{}: {} parts, {} joints", rig.name, rig.parts.size(), character.joint_count())
            .c_str());
    if (ImGui::CollapsingHeader("Parts", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_parts(rig);
    }
    if (ImGui::CollapsingHeader("Joints", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_joints(character);
    }
}

} // namespace

void RigInspectorPanel::draw(const Session& session) {
    const std::span<const aplomb::CharacterId> characters = session.characters();
    if (!inspected_ || std::ranges::find(characters, *inspected_) == characters.end()) {
        inspected_ = characters.empty() ? std::nullopt : std::optional(characters.front());
    }

    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport.WorkPos.x + viewport.WorkSize.x - window_margin,
                             viewport.WorkPos.y + window_margin},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    ImGui::SetNextWindowSize({window_width, window_height}, ImGuiCond_FirstUseEver);
    // It starts folded, so that it doesn't hide the scene until it is wanted.
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Rig inspector")) {
        if (inspected_) {
            inspected_ = choose_character(session, *inspected_);
            draw_rig(session.world().character(*inspected_));
        } else {
            ImGui::TextUnformatted("Add a character to inspect its rig.");
        }
    }
    ImGui::End();
}
