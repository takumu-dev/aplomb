# aplomb: an active ragdoll library on Jolt Physics. aplomb::core holds the world and everything
# that controls its characters, and links Jolt and the standard library alone.
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

# Standard C++23 as ordinary headers: no compiler extensions, and no scanning for C++ modules,
# which aplomb doesn't use. Each target asks for C++23 itself.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# A host that adds aplomb as a subproject gets the library alone.
option(APLOMB_BUILD_TESTS "Build aplomb's tests." ${PROJECT_IS_TOP_LEVEL})

add_subdirectory(src/core)

if(APLOMB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(AplombLint)
endif()
