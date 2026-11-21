/// \file
/// Scenes built by name, as scenario files name them.

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <span>
#include <string_view>

namespace aplomb {

/// The names of every scene there is, such as `flat_ground`, in alphabetical order.
[[nodiscard]] std::span<const std::string_view> scene_names() noexcept;

/// Builds a scene into a physics system by its name.
///
/// \param name The scene's name, such as `flat_ground`.
/// \param physics_system The physics system to add the scene's bodies to, one a world owns.
/// \return Whether there is a scene of that name. If there isn't, nothing is added.
[[nodiscard]] bool build_scene(std::string_view name, JPH::PhysicsSystem& physics_system);

} // namespace aplomb
