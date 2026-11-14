// The sandbox: a window onto a world on flat ground, where the owner loads a rig file, adds
// characters, pushes them and watches them fall from any side, pausing, stepping and changing the
// speed, with debug layers for what the physics is doing and an inspector for the characters' rigs.
//
//   aplomb_sandbox [<rig file>]
//
// The rig file is the one characters are first added from, the humanoid rig in the source tree's
// assets by default; another can be loaded from the sandbox's panel. It exits with 1 when the world
// or the window can't be made, and 2 when it is given more than a rig file.
#include "debug_draw/raylib_debug_draw.hpp"
#include "host_services.hpp"
#include "jolt_runtime.hpp"
#include "sandbox.hpp"
#include "session.hpp"
#include <aplomb/core/world_error.hpp>
#include <aplomb/core/world_settings.hpp>

#include <cstddef>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <utility>

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() > 2) {
        std::println(stderr, "usage: aplomb_sandbox [<rig file>]");
        return 2;
    }
    const std::filesystem::path rig_file =
        arguments.size() == 2 ? std::filesystem::path(arguments[1]) : APLOMB_SANDBOX_RIG_FILE;

    const JoltRuntime jolt;
    HostServices host;
    const aplomb::WorldSettings settings = host.world_settings();
    std::expected<Session, aplomb::WorldError> session = Session::create(settings);
    if (!session) {
        std::println(stderr, "the world couldn't be created: {}", session.error());
        return 1;
    }

    std::expected<RaylibDebugDraw, std::string> window = RaylibDebugDraw::open("aplomb sandbox");
    if (!window) {
        std::println(stderr, "{}", window.error());
        return 1;
    }
    RaylibDebugDraw& backend = *window;

    Sandbox sandbox(std::move(*session), rig_file);
    while (!backend.closing()) {
        backend.begin_frame();
        sandbox.update(backend.frame_seconds());
        backend.begin_scene(sandbox.camera_view());
        sandbox.draw(backend);
        backend.end_scene();
        backend.end_frame();
    }
    return 0;
}
