# A host's project that uses aplomb, as the build's tests make it: against aplomb's installed
# package of APLOMB_VERSION, or with aplomb as a subproject when APLOMB_SOURCE_DIR names aplomb's
# source. Either way it links both libraries by the names a host uses.
cmake_minimum_required(VERSION 4.4)

project(aplomb_host_project LANGUAGES CXX)

if(DEFINED APLOMB_SOURCE_DIR)
    add_subdirectory("${APLOMB_SOURCE_DIR}" aplomb)
else()
    find_package(aplomb ${APLOMB_VERSION} CONFIG REQUIRED)
endif()

add_executable(aplomb_host_project main.cpp)
target_link_libraries(aplomb_host_project PRIVATE aplomb::core aplomb::files)
