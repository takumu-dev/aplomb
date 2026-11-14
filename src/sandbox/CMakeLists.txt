# The sandbox, a viewer in which the owner loads a rig file, adds characters onto flat ground and
# watches them fall. It uses only the public interfaces of aplomb::core, aplomb::files and
# aplomb::scenes, and draws through the DebugDraw interface in debug_draw/, whose one backend is
# raylib's, with its panels in Dear ImGui through rlImGui. Every preset builds it, but nothing opens
# its window.
find_package(imgui CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(rlimgui CONFIG REQUIRED)

add_executable(aplomb_sandbox
    debug_draw/raylib_debug_draw.cpp
    host_services.cpp
    jolt_runtime.cpp
    layers/shapes_layer.cpp
    main.cpp
    orbit_camera.cpp
    panels/characters_panel.cpp
    panels/rig_file_panel.cpp
    panels/stepping_panel.cpp
    sandbox.cpp
    session.cpp
    step_clock.cpp
)
target_include_directories(aplomb_sandbox PRIVATE .)
target_compile_features(aplomb_sandbox PRIVATE cxx_std_23)
target_compile_definitions(aplomb_sandbox PRIVATE
    APLOMB_SANDBOX_RIG_FILE="${PROJECT_SOURCE_DIR}/assets/rigs/humanoid.json"
)
target_link_libraries(aplomb_sandbox PRIVATE
    aplomb::core
    aplomb::files
    aplomb::scenes
    imgui::imgui
    raylib
    rlimgui::rlimgui
)
aplomb_target_warnings(aplomb_sandbox)
# The sandbox works out where the characters it adds are placed, which it feeds the physics.
aplomb_target_deterministic_floating_point(aplomb_sandbox)
