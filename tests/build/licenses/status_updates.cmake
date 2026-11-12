# vcpkg records what it installs and removes in files under vcpkg/updates until it next folds them
# into its status, each paragraph taking the place of any earlier one for the same package. Here
# the status holds a port that has since been removed, and an update holds one since installed.
#
#   INSTALLED  the licence of the port installed through the update

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

fake_vcpkg_manifest(DEPENDENCIES good-lib)
fake_vcpkg_port(good-lib VERSION 1.0 LICENSE MIT)
fake_vcpkg_port(removed-lib VERSION 1.0 LICENSE GPL-3.0-only)
file(MAKE_DIRECTORY "${FAKE_INSTALLED_DIR}/vcpkg/updates")
file(WRITE "${FAKE_INSTALLED_DIR}/vcpkg/updates/0000000000" [=[
Package: removed-lib
Version: 1.0
Architecture: x64-linux
Status: purge ok not-installed

]=])

# fake_vcpkg_port writes its paragraph to the status, so it goes into an update from there.
file(READ "${FAKE_INSTALLED_DIR}/vcpkg/status" status)
fake_vcpkg_port(new-lib VERSION 1.0 LICENSE ${INSTALLED})
file(READ "${FAKE_INSTALLED_DIR}/vcpkg/status" status_with_new_lib)
string(LENGTH "${status}" length)
string(SUBSTRING "${status_with_new_lib}" ${length} -1 paragraph)
file(WRITE "${FAKE_INSTALLED_DIR}/vcpkg/status" "${status}")
file(WRITE "${FAKE_INSTALLED_DIR}/vcpkg/updates/0000000001" "${paragraph}")

fake_vcpkg_check()
