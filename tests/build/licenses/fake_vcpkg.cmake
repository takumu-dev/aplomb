# A fake vcpkg for the licence check's tests: a manifest, the tree `vcpkg install` leaves, and the
# ports it installed, each described as vcpkg's own metadata describes it. Every port is built for
# one triplet, which serves as the host triplet too, as on a native Linux build.
#
# A port's own manifest is an overlay port's, or one in a git registry, found through the tree that
# vcpkg's SPDX document names.

include("${APLOMB_CMAKE_DIR}/AplombLicenses.cmake")

set(FAKE_TRIPLET x64-linux)
set(FAKE_MANIFEST_DIR "${TEST_DIR}/project")
set(FAKE_INSTALLED_DIR "${TEST_DIR}/project/vcpkg_installed")
set(FAKE_REGISTRY "${TEST_DIR}/registry")
set(FAKE_NOTICES "${TEST_DIR}/THIRD_PARTY_NOTICES.md")

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${FAKE_INSTALLED_DIR}/vcpkg" "${FAKE_REGISTRY}")
file(WRITE "${FAKE_INSTALLED_DIR}/vcpkg/status" "")
file(WRITE "${FAKE_MANIFEST_DIR}/vcpkg-configuration.json" [=[{ "overlay-ports": ["./ports"] }]=])

find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" init --quiet "${FAKE_REGISTRY}" COMMAND_ERROR_IS_FATAL ANY)

# Writes a list of dependencies as a manifest's JSON array, each a name, or a name followed by
# ":host" for a host dependency.
function(fake_vcpkg_dependencies out_json)
    set(json "[]")
    set(index 0)
    foreach(dependency IN LISTS ARGN)
        if(dependency MATCHES "^(.*):host$")
            set(entry "{ \"name\": \"${CMAKE_MATCH_1}\", \"host\": true }")
        else()
            set(entry "\"${dependency}\"")
        endif()
        string(JSON json SET "${json}" ${index} "${entry}")
        math(EXPR index "${index} + 1")
    endforeach()
    set(${out_json} "${json}" PARENT_SCOPE)
endfunction()

# Writes the project's manifest.
#
#   DEPENDENCIES       its dependencies, as fake_vcpkg_dependencies takes them
#   TEST_DEPENDENCIES  the dependencies of its `tests` feature
function(fake_vcpkg_manifest)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "DEPENDENCIES;TEST_DEPENDENCIES")
    fake_vcpkg_dependencies(dependencies ${arg_DEPENDENCIES})
    fake_vcpkg_dependencies(test_dependencies ${arg_TEST_DEPENDENCIES})
    set(json "{ \"name\": \"project\", \"features\": { \"tests\": { \"description\": \"\" } } }")
    string(JSON json SET "${json}" dependencies "${dependencies}")
    string(JSON json SET "${json}" features tests dependencies "${test_dependencies}")
    file(WRITE "${FAKE_MANIFEST_DIR}/vcpkg.json" "${json}")
endfunction()

# Installs a port.
#
#   VERSION       its version
#   LICENSE       its licence as its manifest declares it; NOASSERTION, as vcpkg writes a missing one,
#                 by default
#   HOMEPAGE      its homepage, if it declares one
#   DEPENDENCIES  its dependencies, as fake_vcpkg_dependencies takes them
#   REGISTRY      if given, the port comes from the git registry rather than an overlay
function(fake_vcpkg_port name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "REGISTRY" "VERSION;LICENSE;HOMEPAGE" "DEPENDENCIES")
    if(NOT DEFINED arg_LICENSE)
        set(arg_LICENSE NOASSERTION)
    endif()

    fake_vcpkg_dependencies(dependencies ${arg_DEPENDENCIES})
    set(manifest "{ \"name\": \"${name}\", \"version\": \"${arg_VERSION}\" }")
    string(JSON manifest SET "${manifest}" dependencies "${dependencies}")
    if(arg_REGISTRY)
        file(WRITE "${FAKE_REGISTRY}/ports/${name}/vcpkg.json" "${manifest}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${FAKE_REGISTRY}" add "ports/${name}"
            COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${FAKE_REGISTRY}" write-tree "--prefix=ports/${name}/"
            OUTPUT_VARIABLE tree
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        set(download_location "git+https://github.com/Microsoft/vcpkg@${tree}")
    else()
        file(WRITE "${FAKE_MANIFEST_DIR}/ports/${name}/vcpkg.json" "${manifest}")
        set(download_location NOASSERTION)
    endif()

    # vcpkg's status lists host dependencies among the others, with no mark of which is which.
    set(depends "")
    foreach(dependency IN LISTS arg_DEPENDENCIES)
        string(REGEX REPLACE ":host$" "" dependency "${dependency}")
        list(APPEND depends "${dependency}")
    endforeach()
    list(REMOVE_DUPLICATES depends)
    list(JOIN depends ", " depends)
    set(paragraph "Package: ${name}\nVersion: ${arg_VERSION}\n")
    if(depends)
        string(APPEND paragraph "Depends: ${depends}\n")
    endif()
    string(APPEND paragraph "Architecture: ${FAKE_TRIPLET}\nStatus: install ok installed\n\n")
    file(APPEND "${FAKE_INSTALLED_DIR}/vcpkg/status" "${paragraph}")

    set(package "{ \"SPDXID\": \"SPDXRef-port\", \"homepage\": null }")
    string(JSON package SET "${package}" name "\"${name}\"")
    string(JSON package SET "${package}" versionInfo "\"${arg_VERSION}\"")
    string(JSON package SET "${package}" downloadLocation "\"${download_location}\"")
    string(JSON package SET "${package}" licenseConcluded "\"${arg_LICENSE}\"")
    string(JSON package SET "${package}" licenseDeclared "\"NOASSERTION\"")
    if(DEFINED arg_HOMEPAGE)
        string(JSON package SET "${package}" homepage "\"${arg_HOMEPAGE}\"")
    endif()
    set(share "${FAKE_INSTALLED_DIR}/${FAKE_TRIPLET}/share/${name}")
    file(WRITE "${share}/vcpkg.spdx.json" "{ \"spdxVersion\": \"SPDX-2.3\", \"packages\": [${package}] }")
    file(WRITE "${share}/copyright" "Copyright of ${name}.\n")
endfunction()

# Runs the licence check on the fake vcpkg's tree.
function(fake_vcpkg_check)
    aplomb_check_licenses(
        MANIFEST_DIR "${FAKE_MANIFEST_DIR}"
        INSTALLED_DIR "${FAKE_INSTALLED_DIR}"
        TRIPLET "${FAKE_TRIPLET}"
        REGISTRY "${FAKE_REGISTRY}"
        NOTICES "${FAKE_NOTICES}"
    )
endfunction()
