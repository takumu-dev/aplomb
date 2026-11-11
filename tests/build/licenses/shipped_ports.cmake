# Which ports the notices list: what the project depends on, and what that depends on in turn, but
# not the host tools that build them, nor what only the tests need. One port is a host tool of one
# shipped port and a dependency of another, and another is both a host tool and a dependency of the
# same port, which vcpkg's status lists once; both are shipped.
#
#   REGISTRY  ON if the ports come from the git registry, OFF if from overlays

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

if(REGISTRY)
    set(source REGISTRY)
endif()
fake_vcpkg_manifest(DEPENDENCIES alpha-lib project-tool:host TEST_DEPENDENCIES test-lib)
fake_vcpkg_port(alpha-lib VERSION 1.0 LICENSE MIT ${source}
    DEPENDENCIES zeta-lib port-tool:host shared-lib:host dual-lib dual-lib:host
)
fake_vcpkg_port(zeta-lib VERSION 2.0 LICENSE Zlib ${source} DEPENDENCIES shared-lib)
fake_vcpkg_port(shared-lib VERSION 3.0 LICENSE ISC ${source})
fake_vcpkg_port(dual-lib VERSION 7.0 LICENSE MIT ${source})
fake_vcpkg_port(project-tool VERSION 4.0 LICENSE MIT ${source})
fake_vcpkg_port(port-tool VERSION 5.0 LICENSE MIT ${source})
fake_vcpkg_port(test-lib VERSION 6.0 LICENSE BSL-1.0 ${source})
fake_vcpkg_check()

file(READ "${FAKE_NOTICES}" notices)
string(REGEX MATCHALL "\n## [^\n]+" headings "${notices}")
string(REPLACE "\n## " "" headings "${headings}")
set(expected "alpha-lib 1.0;dual-lib 7.0;shared-lib 3.0;zeta-lib 2.0")
if(NOT headings STREQUAL expected)
    message(FATAL_ERROR "The notices should have listed ${expected}, but listed ${headings}.")
endif()
