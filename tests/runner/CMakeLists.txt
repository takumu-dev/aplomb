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
# The run works out where a timeline's pushes land in the world, so its arithmetic has to round the
# same on every target for a scenario's state hash to.
aplomb_target_deterministic_floating_point(aplomb_runner_support)

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

# Runs the runner on one of the scenarios in scenarios/, which test how it runs and fails, at
# 60 Hz.
#
#   SCENARIO   the scenario file's stem
#   MODE       how the runner runs it, `run` by default
#   METRICS    where the metrics file goes, if anywhere in particular
#   ARGUMENTS  the runner's further arguments, as a list
#   EXPECTED_RESULT, STDOUT_MATCHES, STDERR_MATCHES
#              passed to aplomb_add_program_test
function(aplomb_add_runner_test description)
    set(checks EXPECTED_RESULT STDOUT_MATCHES STDERR_MATCHES)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "SCENARIO;MODE;METRICS;${checks}" "ARGUMENTS")
    if(NOT DEFINED arg_MODE)
        set(arg_MODE run)
    endif()
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
        ARGUMENTS ${arg_MODE} "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/${arg_SCENARIO}.json"
            --step-rate 60 --metrics "${arg_METRICS}" ${arg_ARGUMENTS}
        ${options}
    )
endfunction()

aplomb_add_program_test(runner "the runner, given nothing to run, says how it is used"
    PROGRAM aplomb_runner
    EXPECTED_RESULT 2
    STDERR_MATCHES
        "usage: aplomb_runner run <scenario file> --step-rate <steps a second> --metrics <metrics.*aplomb_runner replay <replay file> --rig <rig file> --metrics <metrics file>"
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

aplomb_add_runner_test("a run whose final state hash differs from its golden hash fails, naming both"
    SCENARIO short_drop
    METRICS "${CMAKE_CURRENT_BINARY_DIR}/golden_hash_differs.json"
    ARGUMENTS --golden-hash 0000000000000000
    EXPECTED_RESULT 1
    STDOUT_MATCHES
        "state_hash_final [0-9a-f]+ differs from the golden hash 0000000000000000\nfailed\n$"
)
foreach(hash IN ITEMS 123 0x0123456789abcde 0123456789abcdef0 0123456789abcdeg -123456789abcdef)
    aplomb_add_runner_test("the runner refuses a golden hash of ${hash}"
        SCENARIO short_drop
        METRICS "${CMAKE_CURRENT_BINARY_DIR}/refused.json"
        ARGUMENTS --golden-hash ${hash}
        EXPECTED_RESULT 2
        STDERR_MATCHES "the golden hash must be 16 hexadecimal digits, not \"${hash}\""
    )
endforeach()
aplomb_add_runner_test("the runner's determinism mode passes a scenario whose two runs agree"
    SCENARIO short_drop
    MODE determinism
    METRICS "${CMAKE_CURRENT_BINARY_DIR}/determinism.json"
    EXPECTED_RESULT 0
    STDOUT_MATCHES
        "both runs gave the same state hash before the first step and after each of 60\npassed\n$"
)
# Records a golden hash the way the golden-hashes target does, into a directory of its own, then
# checks that the run it came from meets it.
add_test(NAME "runner: a run meets the golden hash recorded from it"
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=$<TARGET_FILE:aplomb_runner>"
        "-DSCENARIO_DIR=${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
        -DSCENARIO=short_drop
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/recorded_golden_hash"
        "-DRECORDER=${CMAKE_CURRENT_SOURCE_DIR}/record_golden_hashes.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_recorded_golden_hash.cmake"
)

# Plays one of the replays in replays/ back, with the rig file replays/box.json, a single box,
# unless another is given.
#
#   REPLAY     the replay file's path, relative to this directory
#   RIG        the rig file's path
#   METRICS    where the metrics file goes, if anywhere in particular
#   EXPECTED_RESULT, STDOUT_MATCHES, STDERR_MATCHES
#              passed to aplomb_add_program_test
function(aplomb_add_replay_test description)
    set(checks EXPECTED_RESULT STDOUT_MATCHES STDERR_MATCHES)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "REPLAY;RIG;METRICS;${checks}" "")
    if(NOT DEFINED arg_RIG)
        set(arg_RIG "${CMAKE_CURRENT_SOURCE_DIR}/replays/box.json")
    endif()
    if(NOT DEFINED arg_METRICS)
        set(arg_METRICS "${CMAKE_CURRENT_BINARY_DIR}/refused.json")
    endif()
    set(options "")
    foreach(check IN LISTS checks)
        if(DEFINED arg_${check})
            list(APPEND options ${check} "${arg_${check}}")
        endif()
    endforeach()
    aplomb_add_program_test(runner "${description}"
        PROGRAM aplomb_runner
        ARGUMENTS replay "${CMAKE_CURRENT_SOURCE_DIR}/${arg_REPLAY}" --rig "${arg_RIG}"
            --metrics "${arg_METRICS}"
        ${options}
    )
endfunction()

aplomb_add_runner_test("the runner records a replay of a run"
    SCENARIO short_drop
    METRICS "${CMAKE_CURRENT_BINARY_DIR}/recorded.json"
    ARGUMENTS --replay "${CMAKE_CURRENT_BINARY_DIR}/short_drop_replay.json"
    EXPECTED_RESULT 0
    STDOUT_MATCHES "\npassed\n$"
)
set_tests_properties("runner: the runner records a replay of a run"
    PROPERTIES FIXTURES_SETUP runner_recorded_replay
)
aplomb_add_program_test(runner "a replay of a run plays back to the run's state hash at every step"
    PROGRAM aplomb_runner
    ARGUMENTS replay "${CMAKE_CURRENT_BINARY_DIR}/short_drop_replay.json"
        --rig "${PROJECT_SOURCE_DIR}/assets/rigs/humanoid.json"
        --metrics "${CMAKE_CURRENT_BINARY_DIR}/replayed.json"
    EXPECTED_RESULT 0
    STDOUT_MATCHES
        "^short_drop_replay at 60 Hz: 60 steps\nevery state hash was the replay's, before the first step and after each of 60\npassed\n$"
)
set_tests_properties("runner: a replay of a run plays back to the run's state hash at every step"
    PROPERTIES FIXTURES_REQUIRED runner_recorded_replay
)
aplomb_add_replay_test(
    "a replay whose state hashes differ from those played back fails, naming the first step at which they do"
    REPLAY replays/wrong_hashes.json
    EXPECTED_RESULT 1
    STDOUT_MATCHES
        "the replay differed first at step 0, with the recorded state hash 0000000000000000 and the played back [0-9a-f]+\nfailed\n$"
)
aplomb_add_replay_test(
    "the runner says when a replay was recorded by another version of aplomb or Jolt"
    REPLAY replays/wrong_hashes.json
    EXPECTED_RESULT 1
    STDOUT_MATCHES
        "the replay was recorded by aplomb 0\\.0\\.1 with Jolt 5\\.5\\.0, and is played back by aplomb [0-9.]+ with Jolt [0-9.]+\n"
)
aplomb_add_replay_test(
    "the runner refuses to play a replay back with a rig file other than the one it was recorded with"
    REPLAY replays/wrong_hashes.json
    RIG "${PROJECT_SOURCE_DIR}/assets/rigs/humanoid.json"
    EXPECTED_RESULT 2
    STDERR_MATCHES
        "the replay was recorded with a rig file whose hash is e50d54446ab308a3, but the hash of \"[^\"]*humanoid.json\" is [0-9a-f]+"
)
aplomb_add_replay_test("the runner reports every problem in a replay file"
    REPLAY scenarios/short_drop.json
    EXPECTED_RESULT 2
    STDERR_MATCHES
        "short_drop.json\" has 1 problem: .*/format: the format of a replay file is \"aplomb.replay\", not \"aplomb.scenario\""
)
aplomb_add_replay_test("the runner names a rig file it can't read for a replay"
    REPLAY replays/wrong_hashes.json
    RIG "${CMAKE_CURRENT_SOURCE_DIR}/no_such_rig.json"
    EXPECTED_RESULT 2
    STDERR_MATCHES "the rig file \"[^\"]*no_such_rig.json\" can't be read: there is no such file"
)
aplomb_add_replay_test("a replay that doesn't play back still writes its metrics file"
    REPLAY replays/wrong_hashes.json
    METRICS "${CMAKE_CURRENT_BINARY_DIR}/wrong_hashes.json"
    EXPECTED_RESULT 1
)
set_tests_properties("runner: a replay that doesn't play back still writes its metrics file"
    PROPERTIES FIXTURES_SETUP runner_wrong_hashes
)
# Not a program test, since what it checks is the file the playback before it wrote.
add_test(NAME
    "runner: a replay's metrics file names the replay, its step rate and the first step at which it differed"
    COMMAND "${CMAKE_COMMAND}"
        "-DMETRICS=${CMAKE_CURRENT_BINARY_DIR}/wrong_hashes.json"
        -DSCENARIO=wrong_hashes -DSTEP_RATE=60 -DNAMES= -DFIRST_DIFFERING_STEP=0
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_metrics.cmake"
)
set_tests_properties(
    "runner: a replay's metrics file names the replay, its step rate and the first step at which it differed"
    PROPERTIES FIXTURES_REQUIRED runner_wrong_hashes
)
