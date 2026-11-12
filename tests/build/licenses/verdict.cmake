# Checks the licence check's verdict on one SPDX licence expression.
#
#   EXPRESSION  the expression
#   EXPECTED    the verdict it must get: allowed, refused or undecided

include("${APLOMB_CMAKE_DIR}/AplombLicenses.cmake")

aplomb_license_verdict("${EXPRESSION}" verdict)
if(NOT verdict STREQUAL EXPECTED)
    message(FATAL_ERROR "\"${EXPRESSION}\" should have been ${EXPECTED}, but was ${verdict}.")
endif()
