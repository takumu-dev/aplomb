# Builds the host's project in host_project/ against aplomb and runs it through
# support/check_program.cmake, stopping with an error at the first step that fails. The project is
# configured as aplomb's build was, with the same generator, compiler, build type, flags and
# dependencies.
#
#   MODE                  `package`, to install aplomb from APLOMB_BINARY_DIR and find its package,
#                         or `subproject`, to add aplomb's source as a subproject
#   APLOMB_SOURCE_DIR     aplomb's source
#   APLOMB_BINARY_DIR     aplomb's build, which `package` installs
#   APLOMB_VERSION        the version that `package` asks find_package for, such as 0.1
#   APLOMB_EXCEPTIONS     whether `subproject` builds aplomb with exceptions
#   CONFIG                the configuration to install and build
#   GENERATOR, MAKE_PROGRAM, CXX_COMPILER, CXX_FLAGS, EXE_LINKER_FLAGS
#                         how aplomb's build was configured, with the flags it links programs with,
#                         which link its sanitizers' runtimes, if any
#   TOOLCHAIN_FILE, VCPKG_INSTALLED_DIR, VCPKG_TARGET_TRIPLET
#                         where it found its dependencies, if through vcpkg
#   RIG                   the rig file the program reads
#   STDOUT_MATCHES        a regular expression the program's stdout must match

# Runs a command, stopping with its output when it fails.
function(run description)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed with ${result}:\n${output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${TEST_DIR}")
set(prefix "${TEST_DIR}/prefix")
set(build "${TEST_DIR}/build")

set(options
    -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${CXX_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS=${EXE_LINKER_FLAGS}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
)
if(MAKE_PROGRAM)
    list(APPEND options "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(TOOLCHAIN_FILE)
    list(APPEND options
        "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}"
        -DVCPKG_MANIFEST_MODE=OFF
        "-DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
        "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    )
endif()

if(MODE STREQUAL "package")
    run("Installing aplomb"
        "${CMAKE_COMMAND}" --install "${APLOMB_BINARY_DIR}" --prefix "${prefix}" --config "${CONFIG}"
    )
    list(APPEND options "-DCMAKE_PREFIX_PATH=${prefix}" "-DAPLOMB_VERSION=${APLOMB_VERSION}")
elseif(MODE STREQUAL "subproject")
    list(APPEND options
        "-DAPLOMB_SOURCE_DIR=${APLOMB_SOURCE_DIR}"
        "-DAPLOMB_EXCEPTIONS=${APLOMB_EXCEPTIONS}"
    )
else()
    message(FATAL_ERROR "MODE is `package` or `subproject`, not `${MODE}`.")
endif()

run("Configuring the host's project"
    "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/host_project" -B "${build}" ${options}
)
run("Building the host's project" "${CMAKE_COMMAND}" --build "${build}" --config "${CONFIG}")

find_program(PROGRAM aplomb_host_project PATHS "${build}" "${build}/${CONFIG}" NO_DEFAULT_PATH
    REQUIRED
)
set(ARGUMENTS "${RIG}")
include("${CMAKE_CURRENT_LIST_DIR}/../support/check_program.cmake")
