# A shipped port whose SPDX document is missing, or holds no licence, as though it declared none.
#
#   MISSING   `document` or `licence`
#   REVIEWED  ON if a person has reviewed the port's missing licence

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

if(REVIEWED)
    set(APLOMB_REVIEWED_LICENSES bare-lib=NOASSERTION)
endif()
fake_vcpkg_manifest(DEPENDENCIES bare-lib)
fake_vcpkg_port(bare-lib VERSION 1.0 LICENSE MIT)
set(spdx "${FAKE_INSTALLED_DIR}/${FAKE_TRIPLET}/share/bare-lib/vcpkg.spdx.json")
if(MISSING STREQUAL "document")
    file(REMOVE "${spdx}")
else()
    file(READ "${spdx}" document)
    string(JSON document REMOVE "${document}" packages 0 licenseConcluded)
    file(WRITE "${spdx}" "${document}")
endif()
fake_vcpkg_check()

file(READ "${FAKE_NOTICES}" notices)
if(NOT notices MATCHES "\n## bare-lib 1.0\n\n- Licence: NOASSERTION\n")
    message(FATAL_ERROR "The notices should have listed bare-lib under NOASSERTION:\n${notices}")
endif()
