# The checks that build nothing. Each target exists only where its tool is found.
#
#   check-format  fails if any source differs from what clang-format would make of it
#   format        reformats every source in place

include_guard(GLOBAL)

find_program(APLOMB_CLANG_FORMAT clang-format)
if(APLOMB_CLANG_FORMAT)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )
    add_custom_target(check-format
        COMMAND "${APLOMB_CLANG_FORMAT}" --dry-run --Werror ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking the formatting"
        VERBATIM
    )
    add_custom_target(format
        COMMAND "${APLOMB_CLANG_FORMAT}" -i ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Formatting the sources"
        VERBATIM
    )
endif()
