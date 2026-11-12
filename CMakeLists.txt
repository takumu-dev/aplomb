# aplomb: an active ragdoll library on Jolt Physics. aplomb::core holds the world and everything
# that controls its characters, and links Jolt and the standard library alone. aplomb::files reads
# rig files into what the core takes.
#
# Every supported build is a preset in CMakePresets.json, so a developer and CI run the same
# `cmake --workflow --preset <preset>`. Dependencies come only through find_package; the build never
# downloads anything.
cmake_minimum_required(VERSION 4.4)

project(aplomb
    VERSION 0.1.0
    DESCRIPTION "An engine-independent active ragdoll library built on Jolt Physics"
    HOMEPAGE_URL "https://github.com/takumu-dev/aplomb"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AplombCompilerFloor)
include(AplombConventions)

aplomb_check_compiler_floor("${CMAKE_CXX_COMPILER_ID}" "${CMAKE_CXX_COMPILER_VERSION}")

# Every dependency vcpkg installed must be under a licence on the allowlist. The third-party notices
# a host ships come from the same metadata: configuring writes them into the build directory, and
# the `third-party-notices` target copies them over THIRD_PARTY_NOTICES.md, where they are
# committed. A host that supplies the dependencies its own way checks their licences itself.
if(PROJECT_IS_TOP_LEVEL AND VCPKG_MANIFEST_MODE)
    include(AplombLicenses)
    set(APLOMB_THIRD_PARTY_NOTICES "${PROJECT_BINARY_DIR}/THIRD_PARTY_NOTICES.md")
    aplomb_check_licenses(
        MANIFEST_DIR "${VCPKG_MANIFEST_DIR}"
        INSTALLED_DIR "${VCPKG_INSTALLED_DIR}"
        TRIPLET "${VCPKG_TARGET_TRIPLET}"
        # vcpkg's root, where the presets find it, is the git repository of its built-in registry.
        REGISTRY "$ENV{VCPKG_ROOT}"
        NOTICES "${APLOMB_THIRD_PARTY_NOTICES}"
    )
    add_custom_target(third-party-notices
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${APLOMB_THIRD_PARTY_NOTICES}" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        COMMENT "Updating the third-party notices"
        VERBATIM
    )
endif()

# Standard C++23 as ordinary headers: no compiler extensions, and no scanning for C++ modules,
# which aplomb doesn't use. Each target asks for C++23 itself.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# A host that adds aplomb as a subproject gets the library alone.
option(APLOMB_BUILD_TESTS "Build aplomb's tests." ${PROJECT_IS_TOP_LEVEL})
# Many hosts build without exceptions, and the library must build that way too. The tests keep
# exceptions either way, since Catch2 needs them.
option(APLOMB_EXCEPTIONS "Build the library with exceptions enabled." ON)
set(APLOMB_SANITIZERS "" CACHE STRING
    "The sanitizers to build everything with, as a list such as \"address;undefined\"."
)
aplomb_enable_sanitizers("${APLOMB_SANITIZERS}")

add_subdirectory(src/core)
add_subdirectory(src/files)
add_subdirectory(src/scenes)

if(APLOMB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(AplombLint)
endif()
