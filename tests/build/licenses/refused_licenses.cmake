# A dependency and a host tool under licences outside the allowlist, beside one inside it.

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

fake_vcpkg_manifest(DEPENDENCIES good-lib gpl-lib)
fake_vcpkg_port(good-lib VERSION 1.0 LICENSE MIT)
fake_vcpkg_port(gpl-lib VERSION 2.0 LICENSE GPL-3.0-only DEPENDENCIES agpl-tool:host)
fake_vcpkg_port(agpl-tool VERSION 3.0 LICENSE AGPL-3.0-only)
fake_vcpkg_check()
