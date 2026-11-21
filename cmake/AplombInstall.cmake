# Installs the library a host ships, aplomb::core and aplomb::files, as a CMake package that a host
# finds with find_package(aplomb), along with aplomb's licence and the third-party notices. The
# scenes, the runner and the sandbox stay behind.

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/aplomb")

install(TARGETS aplomb_core aplomb_files
    EXPORT aplomb_targets
    FILE_SET HEADERS
)
install(EXPORT aplomb_targets
    NAMESPACE aplomb::
    FILE aplombTargets.cmake
    DESTINATION "${package_dir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/aplombConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/aplombConfig.cmake"
    INSTALL_DESTINATION "${package_dir}"
)
# Before 1.0, a new minor version may break what the last one offered.
write_basic_package_version_file("${PROJECT_BINARY_DIR}/aplombConfigVersion.cmake"
    COMPATIBILITY SameMinorVersion
)
install(FILES
    "${PROJECT_BINARY_DIR}/aplombConfig.cmake"
    "${PROJECT_BINARY_DIR}/aplombConfigVersion.cmake"
    DESTINATION "${package_dir}"
)

install(FILES
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
