# rlImGui, the Dear ImGui backend for raylib that the sandbox's panels go through. It has no
# releases and no CMake build of its own, so the port pins a commit and builds its one source with
# the CMakeLists.txt beside this file. Its Font Awesome icons are left out: the font is under a
# licence outside the allowlist, and the sandbox doesn't use them.
#
# Moving to another commit changes the reference, the date in the version and the hash, in a commit
# of its own.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO raylib-extras/rlImGui
    REF 1550009359ad975927f7f0e4a3f47e3f27123ea9
    SHA512 5c3fb4b756d687b44fe5fc241256232e0a22528a34832d93351dcd282314ad866d2fa242eef1ed08ad5111e89e51e0614d6c19159e3cda3a2ae76ca3591912b9
    HEAD_REF main
)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH share/rlimgui)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
