#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include <aplomb/core/character.hpp>
#include <aplomb/core/joint_desc.hpp>
#include <aplomb/core/part_desc.hpp>
#include <aplomb/core/rig_desc.hpp>
#include <aplomb/core/shape_desc.hpp>
#include <aplomb/core/typed_id.hpp>
#include <aplomb/core/world.hpp>
#include <aplomb/files/rig_file.hpp>
#include <aplomb/scenes/flat_ground.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace {

// How many times heavier than its parent part, or its parent part than it, a part may be.
// Jolt's solver holds a chain of bodies of similar mass far better than one where a light part
// hangs on a heavy one.
constexpr float largest_mass_ratio = 3.0F;

aplomb::RigDesc humanoid() {
    auto rig = aplomb::load_rig_file(std::filesystem::path(APLOMB_RIGS_DIR) / "humanoid.json");
    INFO((rig.has_value() ? std::string() : std::format("{}", rig.error())));
    REQUIRE(rig.has_value());
    return *rig;
}

std::map<std::string, std::reference_wrapper<const aplomb::PartDesc>>
parts_by_name(const aplomb::RigDesc& rig) {
    std::map<std::string, std::reference_wrapper<const aplomb::PartDesc>> parts;
    for (const aplomb::PartDesc& part : rig.parts) {
        parts.emplace(part.name, part);
    }
    return parts;
}

} // namespace

TEST_CASE("the humanoid rig has 16 parts, with the pelvis as its root part", "[files]") {
    const aplomb::RigDesc rig = humanoid();

    std::vector<std::string> names;
    std::vector<std::string> roots;
    for (const aplomb::PartDesc& part : rig.parts) {
        names.push_back(part.name);
        if (!part.parent.has_value()) {
            roots.push_back(part.name);
        }
    }
    std::ranges::sort(names);
    CHECK(names == std::vector<std::string>{
                       "chest",
                       "foot_l",
                       "foot_r",
                       "hand_l",
                       "hand_r",
                       "head",
                       "lower_arm_l",
                       "lower_arm_r",
                       "pelvis",
                       "shin_l",
                       "shin_r",
                       "spine",
                       "thigh_l",
                       "thigh_r",
                       "upper_arm_l",
                       "upper_arm_r",
                   });
    CHECK(roots == std::vector<std::string>{"pelvis"});
}

TEST_CASE("the humanoid rig's elbows and knees are hinges, and its other joints swing and twist",
          "[files]") {
    const aplomb::RigDesc rig = humanoid();

    for (const aplomb::PartDesc& part : rig.parts) {
        if (!part.joint.has_value()) {
            continue;
        }
        INFO("part " << part.name);
        const bool is_elbow_or_knee =
            part.name.starts_with("lower_arm_") || part.name.starts_with("shin_");
        CHECK(std::holds_alternative<aplomb::HingeJointDesc>(*part.joint) == is_elbow_or_knee);
    }
}

TEST_CASE("the humanoid rig's feet are boxes with high friction", "[files]") {
    const aplomb::RigDesc rig = humanoid();
    const auto parts = parts_by_name(rig);

    for (const char* foot : {"foot_l", "foot_r"}) {
        INFO("part " << foot);
        const aplomb::PartDesc& part = parts.at(foot);
        CHECK(std::holds_alternative<aplomb::BoxDesc>(part.shape));
        CHECK(part.friction >= 1.0F);
    }
}

TEST_CASE("each part of the humanoid rig has a mass similar to its parent part's", "[files]") {
    const aplomb::RigDesc rig = humanoid();
    const auto parts = parts_by_name(rig);

    for (const aplomb::PartDesc& part : rig.parts) {
        if (!part.parent.has_value()) {
            continue;
        }
        const aplomb::PartDesc& parent = parts.at(*part.parent);
        INFO(std::format("part {}, {} kg, under {}, {} kg", part.name, part.mass, parent.name,
                         parent.mass));
        CHECK(std::max(part.mass, parent.mass) <=
              largest_mass_ratio * std::min(part.mass, parent.mass));
    }
}

TEST_CASE("a character made from the humanoid rig and dropped onto flat ground comes to rest "
          "within 3 s",
          "[files]") {
    const JoltRuntime runtime;
    HostServices services;
    auto world = aplomb::World::create(services.world_settings());
    REQUIRE(world.has_value());
    aplomb::build_flat_ground(world->physics_system());
    const aplomb::RigDesc rig = humanoid();

    const auto id = world->add_character(rig, {.position = JPH::Vec3(0.0F, 0.2F, 0.0F)});

    INFO((id.has_value() ? std::string() : std::format("{}", id.error())));
    REQUIRE(id.has_value());
    const aplomb::Character& character = world->character(*id);
    // Three seconds at 60 Hz.
    for (int step = 0; step < 180; ++step) {
        world->step();
    }
    // Every part that isn't finite, above the ground and still, with where it is and how fast it
    // moves.
    std::vector<std::string> restless;
    const JPH::BodyInterface& bodies = world->physics_system().GetBodyInterface();
    for (std::size_t part = 0; part < rig.parts.size(); ++part) {
        const JPH::BodyID body =
            character.part_body(aplomb::PartId(static_cast<std::uint32_t>(part)));
        const JPH::Vec3 position = bodies.GetCenterOfMassPosition(body);
        const float speed = bodies.GetLinearVelocity(body).Length();
        const bool at_rest = std::isfinite(position.GetX()) && std::isfinite(position.GetY()) &&
                             std::isfinite(position.GetZ()) && position.GetY() > 0.0F &&
                             speed < 0.01F;
        if (!at_rest) {
            restless.push_back(std::format("{} at [{}, {}, {}], {} m/s", rig.parts[part].name,
                                           position.GetX(), position.GetY(), position.GetZ(),
                                           speed));
        }
    }
    CHECK(restless.empty());
}
