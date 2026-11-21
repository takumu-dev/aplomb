# Dependencies under licences the check can't decide: one that declares none, and one of its own.
#
#   REVIEWED  the ports and licences a person has reviewed, as APLOMB_REVIEWED_LICENSES holds them,
#             but separated by commas

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

string(REPLACE "," ";" APLOMB_REVIEWED_LICENSES "${REVIEWED}")
fake_vcpkg_manifest(DEPENDENCIES bare-lib odd-lib)
fake_vcpkg_port(bare-lib VERSION 1.0)
fake_vcpkg_port(odd-lib VERSION 2.0 LICENSE LicenseRef-Odd)
fake_vcpkg_check()
