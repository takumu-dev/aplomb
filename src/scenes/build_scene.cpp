#include <aplomb/scenes/build_scene.hpp>

#include <aplomb/scenes/flat_ground.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace aplomb {

namespace {

// A scene, and what builds it.
struct Scene {
    std::string_view name;
    void (*build)(JPH::PhysicsSystem& physics_system);
};

// Every scene, in alphabetical order.
constexpr std::array scenes{
    Scene{
        .name = "flat_ground",
        .build = [](JPH::PhysicsSystem& physics_system) { build_flat_ground(physics_system); },
    },
};

constexpr auto names = [] {
    std::array<std::string_view, scenes.size()> all{};
    std::ranges::transform(scenes, all.begin(), &Scene::name);
    return all;
}();

} // namespace

std::span<const std::string_view> scene_names() noexcept { return names; }

bool build_scene(std::string_view name, JPH::PhysicsSystem& physics_system) {
    const auto* const scene = std::ranges::find(scenes, name, &Scene::name);
    if (scene == scenes.end()) {
        return false;
    }
    scene->build(physics_system);
    return true;
}

} // namespace aplomb
