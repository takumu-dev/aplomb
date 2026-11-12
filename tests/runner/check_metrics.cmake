# Checks a metrics file the runner wrote: its format, the scenario and step rate it names, and that
# it gives the metrics it must, in order.
#
#   METRICS    the metrics file
#   SCENARIO   the scenario's name
#   STEP_RATE  the step rate, as the file writes it
#   NAMES      the metrics' names, as a list

file(READ "${METRICS}" text)
message("${METRICS}:\n${text}")

foreach(key_and_expected IN ITEMS
    "format=aplomb.metrics" "scenario=${SCENARIO}" "step_rate=${STEP_RATE}"
)
    string(REPLACE "=" ";" key_and_expected "${key_and_expected}")
    list(GET key_and_expected 0 key)
    list(GET key_and_expected 1 expected)
    string(JSON actual GET "${text}" "${key}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "The metrics file's ${key} should have been ${expected}, not ${actual}")
    endif()
endforeach()

string(JSON count LENGTH "${text}" metrics)
set(names "")
if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON name MEMBER "${text}" metrics ${index})
        list(APPEND names "${name}")
    endforeach()
endif()
if(NOT names STREQUAL NAMES)
    message(FATAL_ERROR "The metrics file should have given ${NAMES}, not ${names}")
endif()
