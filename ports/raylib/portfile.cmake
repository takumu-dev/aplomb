# raylib as the sandbox draws with it: the desktop platform on OpenGL 3.3 core, with the GLFW and
# the single-header libraries it bundles, and without its audio module. vcpkg's own port builds
# GLFW from vcpkg's glfw3, which on Linux brings in vcpkg's X11 ports and their build tools, under
# licences outside the allowlist. The bundled GLFW is built against the system's X11 and OpenGL
# libraries instead, which nothing ships. raylib's bundled libraries are under Zlib, MIT, or MIT or
# the Unlicense at the user's choice, all within the allowlist, bar the dirent.h that only MSVC
# builds compile, which is under a permissive licence of its own.
#
# The tag is pinned by the version, and its archive by the hash. Moving to another release changes
# both, in a commit of its own.
if(VCPKG_TARGET_IS_LINUX)
    message(STATUS
        "raylib needs the headers of the system's X11 and OpenGL libraries, which Debian has in "
        "libgl-dev, libx11-dev, libxcursor-dev, libxi-dev, libxinerama-dev and libxrandr-dev."
    )
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO raysan5/raylib
    REF "${VERSION}"
    SHA512 dfdb2578859e28d424b6f318669579ab2361c35c4d45e0fee5a80b4c1d7d487a0e084ec2bafffad149b3dd96d34d092586fc3d30c8de6a211b0a2a038a959c07
    HEAD_REF master
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" BUILD_SHARED)

# raylib's CUSTOMIZE_BUILD reads src/config.h as though every feature it defines were on, those it
# defines as 0 included, which turns on custom frame control and leaves EndDrawing() never showing
# a frame. The build keeps config.h's own defaults instead, and turns the audio module off through
# the definition config.h defaults it from, so that raudio.c compiles to nothing. vcpkg takes C
# flags only with C++ flags, though raylib has no C++.
string(APPEND VCPKG_C_FLAGS " -DSUPPORT_MODULE_RAUDIO=0")
string(APPEND VCPKG_CXX_FLAGS " -DSUPPORT_MODULE_RAUDIO=0")
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DBUILD_SHARED_LIBS=${BUILD_SHARED}
        -DCMAKE_POLICY_DEFAULT_CMP0072=NEW # Prefer GLVND
        -DGLFW_BUILD_WAYLAND=OFF
        -DGLFW_BUILD_X11=ON
        -DOPENGL_VERSION=3.3
        -DPLATFORM=Desktop
        -DUSE_EXTERNAL_GLFW=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/raylib.h" "defined(USE_LIBTYPE_SHARED)" "1")
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
