# The scenario runner, which runs a scenario file headlessly at a step rate it is given, writes what
# the run measured to a metrics file, and fails when the scenario's pass criteria do.

add_library(aplomb_runner_support STATIC
    metric_recorder.cpp
    scenario_run.cpp
)
target_include_directories(aplomb_runner_support PUBLIC .)
target_link_libraries(aplomb_runner_support
    PUBLIC aplomb::core aplomb::files aplomb::scenes aplomb_test_support
)
aplomb_target_warnings(aplomb_runner_support)

aplomb_add_tests(runner
    SOURCES
        metric_recorder_tests.cpp
        scenario_run_tests.cpp
    LIBRARIES
        aplomb_runner_support
)

add_executable(aplomb_runner main.cpp)
target_link_libraries(aplomb_runner PRIVATE aplomb_runner_support)
aplomb_target_warnings(aplomb_runner)

# Runs the runner on one of the scenarios in scenarios/, which test how it fails, at 60 Hz.
#
#   SCENARIO  the scenario file's stem
#   METRICS   where the metrics file goes, if anywhere in particular
#   EXPECTED_RESULT, STDOUT_MATCHES, STDERR_MATCHES
#             passed to aplomb_add_program_test
function(aplomb_add_runner_test description)
    set(checks EXPECTED_RESULT STDOUT_MATCHES STDERR_MATCHES)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "SCENARIO;METRICS;${checks}" "")
    if(NOT DEFINED arg_METRICS)
        set(arg_METRICS "${CMAKE_CURRENT_BINARY_DIR}/${arg_SCENARIO}.json")
    endif()
    set(options "")
    foreach(check IN LISTS checks)
        if(DEFINED arg_${check})
            list(APPEND options ${check} "${arg_${check}}")
        endif()
    endforeach()
    aplomb_add_program_test(runner "${description}"
        PROGRAM aplomb_runner
        ARGUMENTS run "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/${arg_SCENARIO}.json"
            --step-rate 60 --metrics "${arg_METRICS}"
        ${options}
    )
endfunction()

aplomb_add_program_test(runner "the runner, given nothing to run, says how it is used"
    PROGRAM aplomb_runner
    EXPECTED_RESULT 2
    STDERR_MATCHES
        "usage: aplomb_runner run <scenario file> --step-rate <steps a second> --metrics <metrics"
)
foreach(rate IN ITEMS fast 0 -60 inf 60hz)
    aplomb_add_program_test(runner "the runner refuses a step rate of ${rate}"
        PROGRAM aplomb_runner
        ARGUMENTS run "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/missed_criterion.json"
            --step-rate ${rate} --metrics "${CMAKE_CURRENT_BINARY_DIR}/refused.json"
        EXPECTED_RESULT 2
        STDERR_MATCHES
            "the step rate must be a positive, finite number of steps a second, not \"${rate}\""
    )
endforeach()
aplomb_add_program_test(runner "the runner refuses to run without a metrics file to write"
    PROGRAM aplomb_runner
    ARGUMENTS run "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/missed_criterion.json" --step-rate 60
    EXPECTED_RESULT 2
    STDERR_MATCHES "usage: aplomb_runner run"
)
aplomb_add_runner_test("the runner names a scenario file it can't read"
    SCENARIO no_such_scenario
    EXPECTED_RESULT 2
    STDERR_MATCHES
        "the scenario file \"[^\"]*no_such_scenario.json\" can't be read: there is no such file"
)
aplomb_add_runner_test("the runner reports every problem in a scenario file"
    SCENARIO two_problems
    EXPECTED_RESULT 2
    STDERR_MATCHES "two_problems.json\" has 2 problems: .*/duration_s: .*/step_rate: "
)
aplomb_add_runner_test("the runner finds a scenario's rig file beside the scenario file"
    SCENARIO missing_rig
    EXPECTED_RESULT 2
    STDERR_MATCHES "the rig file \"[^\"]*runner/scenarios/no_such_rig.json\" can't be read"
)
aplomb_add_runner_test("the runner names a scene that doesn't exist"
    SCENARIO unknown_scene
    EXPECTED_RESULT 2
    STDERR_MATCHES "there is no scene \"moon\""
)
aplomb_add_runner_test("a run that misses a pass criterion fails, naming the criterion"
    SCENARIO missed_criterion
    METRICS "${CMAKE_CURRENT_BINARY_DIR}/missed_criterion.json"
    EXPECTED_RESULT 1
    STDOUT_MATCHES "max_speed_after_3s gt 1000: missed.*nan_count eq 0: met.*\nfailed\n$"
)
set_tests_properties("runner: a run that misses a pass criterion fails, naming the criterion"
    PROPERTIES FIXTURES_SETUP runner_missed_criterion
)
# Not a program test, since what it checks is the file the run before it wrote.
add_test(NAME "runner: a failed run still writes its metrics file"
    COMMAND "${CMAKE_COMMAND}"
        "-DMETRICS=${CMAKE_CURRENT_BINARY_DIR}/missed_criterion.json"
        -DSCENARIO=missed_criterion -DSTEP_RATE=60
        "-DNAMES=max_speed_after_3s$<SEMICOLON>nan_count"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_metrics.cmake"
)
set_tests_properties("runner: a failed run still writes its metrics file"
    PROPERTIES FIXTURES_REQUIRED runner_missed_criterion
)
