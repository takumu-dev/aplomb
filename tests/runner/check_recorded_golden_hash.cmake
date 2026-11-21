# Records a scenario's golden hash at 60 Hz with record_golden_hashes.cmake, into a directory of its
# own, then runs the scenario against that golden hash, which it must meet.
#
#   RUNNER        the scenario runner
#   SCENARIO_DIR  the directory of the scenario files
#   SCENARIO      the scenario file's stem
#   WORK_DIR      the directory to record into, emptied first
#   RECORDER      record_golden_hashes.cmake

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(SCENARIOS "${SCENARIO}")
set(STEP_RATES 60)
set(GOLDEN_DIR "${WORK_DIR}")
set(METRICS_DIR "${WORK_DIR}")
include("${RECORDER}")

file(STRINGS "${WORK_DIR}/${SCENARIO}_60hz.txt" hash)
if(NOT hash MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "The recorded golden hash should have been hexadecimal, not ${hash}")
endif()

execute_process(
    COMMAND "${RUNNER}" run "${SCENARIO_DIR}/${SCENARIO}.json" --step-rate 60
        --metrics "${WORK_DIR}/checked.json" --golden-hash ${hash}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
message("stdout:\n${stdout}")
message("stderr:\n${stderr}")
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The run should have met its recorded golden hash, but it ended with: ${result}")
endif()
