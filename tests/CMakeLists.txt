# aplomb's tests. Each test's name begins with the target it tests, matching the tag on its Catch2
# test case. No test opens a window.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

set(APLOMB_TEST_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/support")

# What the tests share: Jolt's process-wide setup, the job system and temporary allocator a host
# passes to a world, and rigs built in code.
add_library(aplomb_test_support STATIC
    support/host_services.cpp
    support/jolt_runtime.cpp
    support/rigs.cpp
)
target_include_directories(aplomb_test_support PUBLIC support)
target_link_libraries(aplomb_test_support PUBLIC aplomb::core)
aplomb_target_warnings(aplomb_test_support)

# Adds a target's Catch2 test cases as one program, aplomb_<name>_tests, registering each case with
# CTest under the target's name.
#
#   SOURCES    the program's sources
#   LIBRARIES  what it links
function(aplomb_add_tests name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    set(target aplomb_${name}_tests)
    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE ${arg_LIBRARIES} aplomb_test_support Catch2::Catch2WithMain)
    aplomb_target_warnings(${target})
    catch_discover_tests(${target} TEST_PREFIX "${name}: ")
endfunction()

# Builds a test program of a target from one source, naming the program after the source.
function(aplomb_add_test_program out_target name source)
    cmake_path(GET source STEM stem)
    set(target aplomb_${name}_${stem})
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_link_libraries(${target} PRIVATE ${ARGN})
    aplomb_target_warnings(${target})
    set(${out_target} ${target} PARENT_SCOPE)
endfunction()

# Adds a test that runs a program and checks how it ends, through support/check_program.cmake.
#
#   SOURCE     the program's one source file
#   LIBRARIES  what the program links
#   PROGRAM    instead of SOURCE and LIBRARIES, a program target built elsewhere
#   EXPECTED_RESULT, EXPECTED_STDERR, STDOUT_MATCHES, STDERR_MATCHES
#              passed to the script, which describes them; ARGUMENTS too, as a list
function(aplomb_add_program_test name description)
    set(checks EXPECTED_RESULT EXPECTED_STDERR STDOUT_MATCHES STDERR_MATCHES)
    cmake_parse_arguments(PARSE_ARGV 2 arg
        "" "SOURCE;PROGRAM;${checks}" "LIBRARIES;ARGUMENTS"
    )
    if(DEFINED arg_PROGRAM)
        set(program ${arg_PROGRAM})
    else()
        aplomb_add_test_program(program ${name} ${arg_SOURCE} ${arg_LIBRARIES})
    endif()
    if(DEFINED arg_EXPECTED_STDERR)
        cmake_path(ABSOLUTE_PATH arg_EXPECTED_STDERR)
    endif()

    set(options "-DPROGRAM=$<TARGET_FILE:${program}>")
    if(DEFINED arg_ARGUMENTS)
        list(JOIN arg_ARGUMENTS "$<SEMICOLON>" arguments)
        list(APPEND options "-DARGUMENTS=${arguments}")
    endif()
    foreach(check IN LISTS checks)
        if(DEFINED arg_${check})
            list(APPEND options "-D${check}=${arg_${check}}")
        endif()
    endforeach()

    add_test(NAME "${name}: ${description}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${APLOMB_TEST_SUPPORT_DIR}/check_program.cmake"
    )
endfunction()

# The step rates every scenario runs at, and where their golden hashes are kept, each in a file
# named after the scenario and the step rate. Golden hashes are recorded on Linux x86-64 in
# release, so only the presets of builds that must give the same hashes check them.
set(APLOMB_SCENARIO_STEP_RATES 60 120)
set(APLOMB_GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/golden")
option(APLOMB_GOLDEN_HASHES "Check each scenario's final state hash against its golden hash." OFF)
if(APLOMB_GOLDEN_HASHES AND EXISTS "${APLOMB_GOLDEN_DIR}")
    # So that a golden hash recorded where there was none is checked from the next build on.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${APLOMB_GOLDEN_DIR}")
endif()

# Adds the tests of a scenario file in tests/scenarios/: a run of it by aplomb_runner at each of
# the step rates. Each run writes its metrics file to metrics/ in the build directory, named after
# the scenario and the step rate. Where APLOMB_GOLDEN_HASHES is on, each run's final state hash
# must be its golden hash, and a scenario without one fails until the golden-hashes target records
# it.
#
#   SCENARIO     the scenario file's stem
#   DETERMINISM  run it twice at each step rate, failing where the two runs' state hashes differ
#                at any step
function(aplomb_add_scenario_test scenario)
    cmake_parse_arguments(PARSE_ARGV 1 arg "DETERMINISM" "" "")
    set(mode run)
    if(arg_DETERMINISM)
        set(mode determinism)
    endif()
    set_property(GLOBAL APPEND PROPERTY APLOMB_SCENARIOS ${scenario})
    set(metrics_dir "${PROJECT_BINARY_DIR}/metrics")
    file(MAKE_DIRECTORY "${metrics_dir}")
    foreach(rate IN LISTS APLOMB_SCENARIO_STEP_RATES)
        set(name "scenarios: ${scenario} at ${rate} Hz")
        set(golden_options "")
        if(APLOMB_GOLDEN_HASHES)
            set(golden "${APLOMB_GOLDEN_DIR}/${scenario}_${rate}hz.txt")
            if(EXISTS "${golden}")
                file(STRINGS "${golden}" golden_hash LIMIT_COUNT 1)
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${golden}")
                set(golden_options --golden-hash "${golden_hash}")
            else()
                add_test(NAME "${name} has a golden hash"
                    COMMAND "${CMAKE_COMMAND}" -E echo "there is no golden hash at ${golden}; "
                        "build the golden-hashes target to record it"
                )
                set_tests_properties("${name} has a golden hash" PROPERTIES
                    FAIL_REGULAR_EXPRESSION "there is no golden hash"
                )
            endif()
        endif()
        add_test(NAME "${name}"
            COMMAND aplomb_runner ${mode} "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario}.json"
                --step-rate ${rate} --metrics "${metrics_dir}/${scenario}_${rate}hz.json"
                ${golden_options}
        )
    endforeach()
endfunction()

# Adds a test of the build's own CMake code: a script, run through support/check_program.cmake,
# that includes the modules it tests from APLOMB_CMAKE_DIR and stops with an error when the
# behaviour it checks is wrong. It gets a directory of its own to work in as TEST_DIR.
#
#   SCRIPT       the script
#   DEFINITIONS  the other variables it reads, each as NAME=VALUE
#   EXPECTED_RESULT, STDERR_MATCHES
#                passed to the script, which describes them
function(aplomb_add_script_test name description)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SCRIPT;EXPECTED_RESULT;STDERR_MATCHES" "DEFINITIONS")
    cmake_path(ABSOLUTE_PATH arg_SCRIPT)
    string(MAKE_C_IDENTIFIER "${description}" test_dir)
    set(test_dir "${CMAKE_CURRENT_BINARY_DIR}/script_tests/${test_dir}")

    set(arguments
        "-DAPLOMB_CMAKE_DIR=${PROJECT_SOURCE_DIR}/cmake"
        "-DTEST_DIR=${test_dir}"
    )
    foreach(definition IN LISTS arg_DEFINITIONS)
        list(APPEND arguments "-D${definition}")
    endforeach()
    list(APPEND arguments -P "${arg_SCRIPT}")
    list(JOIN arguments "$<SEMICOLON>" arguments)

    set(options "-DPROGRAM=${CMAKE_COMMAND}" "-DARGUMENTS=${arguments}")
    foreach(check IN ITEMS EXPECTED_RESULT STDERR_MATCHES)
        if(DEFINED arg_${check})
            list(APPEND options "-D${check}=${arg_${check}}")
        endif()
    endforeach()

    add_test(NAME "${name}: ${description}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${APLOMB_TEST_SUPPORT_DIR}/check_program.cmake"
    )
endfunction()

# The main program that every precondition test shares.
add_library(aplomb_precondition_main OBJECT support/precondition_main.cpp)
target_include_directories(aplomb_precondition_main PUBLIC support)
target_link_libraries(aplomb_precondition_main PUBLIC aplomb::core)
aplomb_target_warnings(aplomb_precondition_main)

# Adds a test that a precondition fires. A failed check ends the process, so each such test is a
# program of its own, which CTest expects to fail. The source defines break_precondition() from
# support/precondition_test.hpp.
#
#   SOURCE     the program's one source file
#   MESSAGE    the message of the check that must fail, so that no other check passes the test
#   LIBRARIES  what the program links besides core and the tests' support
function(aplomb_add_precondition_test name description)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SOURCE;MESSAGE" "LIBRARIES")
    aplomb_add_test_program(target ${name} ${arg_SOURCE}
        aplomb_precondition_main aplomb_test_support ${arg_LIBRARIES}
    )
    add_test(NAME "${name}: ${description}" COMMAND ${target} "${arg_MESSAGE}")
    set_tests_properties("${name}: ${description}" PROPERTIES WILL_FAIL TRUE)
endfunction()

# Adds a test that a source built with the library's settings fails to compile, and fails for the
# reason the test names, so that no other error passes it. The source is built only by the test.
#
#   SOURCE   the source that must not compile
#   MATCHES  a regular expression the compiler's output must match
function(aplomb_add_compile_failure_test name description)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SOURCE;MATCHES" "")
    cmake_path(GET arg_SOURCE STEM stem)
    set(target aplomb_${name}_${stem})
    add_library(${target} OBJECT EXCLUDE_FROM_ALL ${arg_SOURCE})
    target_compile_features(${target} PRIVATE cxx_std_23)
    aplomb_target_conventions(${target})
    add_test(NAME "${name}: ${description}"
        COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target ${target}
    )
    set_tests_properties("${name}: ${description}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${arg_MATCHES}"
    )
endfunction()

add_subdirectory(core)
add_subdirectory(files)
add_subdirectory(scenes)
add_subdirectory(runner)
add_subdirectory(scenarios)
add_subdirectory(build)

# Records every scenario's golden hash at every step rate in golden/, replacing what is there: the
# only way golden hashes are made. They are recorded on Linux x86-64 with linux-clang-release.
get_property(scenarios GLOBAL PROPERTY APLOMB_SCENARIOS)
list(JOIN scenarios "$<SEMICOLON>" scenarios)
list(JOIN APLOMB_SCENARIO_STEP_RATES "$<SEMICOLON>" step_rates)
add_custom_target(golden-hashes
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=$<TARGET_FILE:aplomb_runner>"
        "-DSCENARIO_DIR=${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
        "-DSCENARIOS=${scenarios}"
        "-DSTEP_RATES=${step_rates}"
        "-DGOLDEN_DIR=${APLOMB_GOLDEN_DIR}"
        "-DMETRICS_DIR=${PROJECT_BINARY_DIR}/metrics"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/runner/record_golden_hashes.cmake"
    COMMENT "Recording every scenario's golden hashes"
    VERBATIM
)
add_dependencies(golden-hashes aplomb_runner)
