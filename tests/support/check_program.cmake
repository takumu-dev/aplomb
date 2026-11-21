# Runs a test program and checks how it ends, for the behaviour a Catch2 test case can't hold: a
# program that must abort, or what a program writes to stdout and stderr.
#
#   PROGRAM          the program to run
#   ARGUMENTS        its arguments, as a list
#   EXPECTED_RESULT  `abort`, or the exit status it must end with; 0 by default
#   EXPECTED_STDERR  a file holding exactly what it must write to stderr
#   STDOUT_MATCHES   a regular expression its stdout must match
#   STDERR_MATCHES   a regular expression its stderr must match
#
# CTest itself counts an abort as a crash, even in a test expected to fail, so a test of an abort
# runs through this script.

if(NOT DEFINED EXPECTED_RESULT)
    set(EXPECTED_RESULT 0)
endif()

execute_process(
    COMMAND "${PROGRAM}" ${ARGUMENTS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
string(REPLACE "\r\n" "\n" stdout "${stdout}")
string(REPLACE "\r\n" "\n" stderr "${stderr}")
message("stdout:\n${stdout}")
message("stderr:\n${stderr}")

if(EXPECTED_RESULT STREQUAL "abort")
    # POSIX reports the signal; on Windows, abort() exits with status 3.
    if(NOT (result MATCHES "aborted" OR (CMAKE_HOST_WIN32 AND result STREQUAL "3")))
        message(FATAL_ERROR "The program should have aborted, but it ended with: ${result}")
    endif()
elseif(NOT result STREQUAL EXPECTED_RESULT)
    message(FATAL_ERROR "The program should have ended with ${EXPECTED_RESULT}, but it ended with: ${result}")
endif()

if(DEFINED EXPECTED_STDERR)
    file(READ "${EXPECTED_STDERR}" expected)
    string(REPLACE "\r\n" "\n" expected "${expected}")
    if(NOT stderr STREQUAL expected)
        message(FATAL_ERROR "stderr should have been exactly:\n${expected}")
    endif()
endif()

if(DEFINED STDOUT_MATCHES AND NOT stdout MATCHES "${STDOUT_MATCHES}")
    message(FATAL_ERROR "stdout should have matched: ${STDOUT_MATCHES}")
endif()

if(DEFINED STDERR_MATCHES AND NOT stderr MATCHES "${STDERR_MATCHES}")
    message(FATAL_ERROR "stderr should have matched: ${STDERR_MATCHES}")
endif()
