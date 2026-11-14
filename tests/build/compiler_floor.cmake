# Checks a compiler against aplomb's compiler floors, as configuring does.
#
#   COMPILER_ID       the compiler's CMake compiler id, such as AppleClang
#   COMPILER_VERSION  its version, as CMake reports it

include("${APLOMB_CMAKE_DIR}/AplombCompilerFloor.cmake")

aplomb_check_compiler_floor("${COMPILER_ID}" "${COMPILER_VERSION}")
