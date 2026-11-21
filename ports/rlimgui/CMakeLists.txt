# Builds rlImGui as a static library, rlimgui::rlimgui, linking vcpkg's Dear ImGui and raylib, and
# installs it with a CMake package of its own.
cmake_minimum_required(VERSION 3.25)

project(rlimgui LANGUAGES C CXX)

find_package(imgui CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)

add_library(rlimgui STATIC rlImGui.cpp)
add_library(rlimgui::rlimgui ALIAS rlimgui)
target_include_directories(rlimgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
# rlImGui.h declares the icon font only without this, so whatever includes it needs it too.
target_compile_definitions(rlimgui PUBLIC NO_FONT_AWESOME)
target_link_libraries(rlimgui PUBLIC imgui::imgui raylib)
target_compile_features(rlimgui PUBLIC cxx_std_11)

install(TARGETS rlimgui EXPORT rlimgui-targets ARCHIVE DESTINATION lib)
install(FILES imgui_impl_raylib.h rlImGui.h rlImGuiColors.h DESTINATION include)
install(EXPORT rlimgui-targets NAMESPACE rlimgui:: DESTINATION share/rlimgui)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rlimgui-config.cmake" [[
include(CMakeFindDependencyMacro)
find_dependency(imgui CONFIG)
find_dependency(raylib CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/rlimgui-targets.cmake")
]])
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rlimgui-config.cmake" DESTINATION share/rlimgui)
