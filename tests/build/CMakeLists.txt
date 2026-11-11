# Tests of the build itself: that the settings a preset turns on take hold. They belong to no target,
# so they are named after the build.

if("address" IN_LIST APLOMB_SANITIZERS)
    aplomb_add_program_test(build "AddressSanitizer stops a read past the end of a heap allocation"
        SOURCE programs/heap_overflow.cpp
        EXPECTED_RESULT 1
        STDERR_MATCHES "ERROR: AddressSanitizer: heap-buffer-overflow"
    )
endif()

if("undefined" IN_LIST APLOMB_SANITIZERS)
    aplomb_add_program_test(build "UndefinedBehaviorSanitizer stops a signed integer overflow"
        SOURCE programs/signed_overflow.cpp
        EXPECTED_RESULT 1
        STDERR_MATCHES "runtime error: signed integer overflow"
    )
endif()

if(NOT APLOMB_EXCEPTIONS)
    aplomb_add_compile_failure_test(build "the library's settings reject a throw when exceptions are off"
        SOURCE programs/stray_throw.cpp
        # GCC's message, then Clang's.
        MATCHES "exception handling disabled|cannot use 'throw' with exceptions disabled"
    )
endif()
