# Records scenarios' golden hashes, which the golden-hashes target alone does: runs each scenario at
# each step rate and writes the final state hash its metrics file gives to
# <scenario>_<rate>hz.txt in the golden directory, removing any other golden hash there. Every run
# must pass and record state_hash_final, or no golden hash is written at all.
#
#   RUNNER        the scenario runner
#   SCENARIO_DIR  the directory of the scenario files
#   SCENARIOS     the scenario files' stems, as a list
#   STEP_RATES    the step rates, as a list
#   GOLDEN_DIR    the directory the golden hashes go in
#   METRICS_DIR   the directory the runs' metrics files go in

set(names "")
set(hashes "")
set(failures "")
foreach(scenario IN LISTS SCENARIOS)
    foreach(rate IN LISTS STEP_RATES)
        set(name "${scenario}_${rate}hz")
        set(metrics "${METRICS_DIR}/${name}.json")
        execute_process(
            COMMAND "${RUNNER}" run "${SCENARIO_DIR}/${scenario}.json"
                --step-rate ${rate} --metrics "${metrics}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
        )
        if(NOT result STREQUAL "0")
            string(APPEND failures "\n${scenario} at ${rate} Hz doesn't pass:\n${stdout}${stderr}")
            continue()
        endif()
        file(READ "${metrics}" text)
        string(JSON hash ERROR_VARIABLE error GET "${text}" metrics state_hash_final)
        if(error)
            string(APPEND failures "\n${scenario} doesn't record state_hash_final\n")
            continue()
        endif()
        list(APPEND names "${name}")
        list(APPEND hashes "${hash}")
    endforeach()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "No golden hash was recorded, since not every run gives one:${failures}")
endif()

file(GLOB previous_files "${GOLDEN_DIR}/*.txt")
foreach(name hash IN ZIP_LISTS names hashes)
    set(golden "${GOLDEN_DIR}/${name}.txt")
    list(REMOVE_ITEM previous_files "${golden}")
    set(previous "")
    if(EXISTS "${golden}")
        file(STRINGS "${golden}" previous LIMIT_COUNT 1)
    endif()
    if(previous STREQUAL hash)
        message(STATUS "${name}: ${hash}, as before")
    elseif(previous STREQUAL "")
        file(WRITE "${golden}" "${hash}\n")
        message(STATUS "${name}: ${hash}, recorded")
    else()
        file(WRITE "${golden}" "${hash}\n")
        message(STATUS "${name}: ${hash}, replacing ${previous}")
    endif()
endforeach()
foreach(stale IN LISTS previous_files)
    file(REMOVE "${stale}")
    message(STATUS "${stale}: removed, since no scenario has it")
endforeach()
