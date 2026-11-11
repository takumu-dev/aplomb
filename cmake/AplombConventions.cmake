# The compiler settings every target of the library and the tests builds with.
#
# The settings of a target are PRIVATE to it, and the sanitizers apply only to aplomb's own
# directories, so none of it reaches a host's build through the library's interface. Warnings become errors only through CMAKE_COMPILE_WARNING_AS_ERROR, which
# the project's own presets set; a host building aplomb as a subproject keeps its own choice.

include_guard(GLOBAL)

# Turns on the strict warnings. Third-party headers come in as SYSTEM includes, so these apply to
# aplomb's own code alone.
function(aplomb_target_warnings target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /w14242 # conversion from one integer type to a smaller one
            /w14254 # a larger bit field assigned to a smaller one
            /w14263 # a member function hides a virtual one
            /w14265 # a class with virtual functions has a non-virtual destructor
            /w14287 # an unsigned and a negative constant compared
            /we4289 # a for-loop variable used outside the loop
            /w14296 # an expression that is always true or false
            /w14311 # a pointer truncated
            /w14545 /w14546 /w14547 /w14549 # a comma operator with no effect
            /w14555 # an expression with no effect
            /w14619 # a pragma naming a warning that doesn't exist
            /w14640 # a local static that isn't thread-safe to construct
            /w14826 # a sign-extending conversion
            /w14905 /w14906 # a string literal cast to the wrong kind of string
            /w14928 # more than one user-defined conversion
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wold-style-cast
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Checks each Doxygen comment against the declaration it documents.
            target_compile_options(${target} PRIVATE -Wdocumentation)
        endif()
    endif()
endfunction()

# Applies the settings of the library's own code: the strict warnings, and floating point that
# gives the same results with every compiler, as Jolt's deterministic build does. Floating-point
# operations are never fused into multiply-adds, which some targets round differently, and never
# reordered by fast math. With APLOMB_EXCEPTIONS off, exceptions are disabled too, so that a stray
# throw or try fails to compile. The tests use only the warnings.
function(aplomb_target_conventions target)
    aplomb_target_warnings(${target})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /fp:precise)
        if(NOT APLOMB_EXCEPTIONS)
            message(FATAL_ERROR "APLOMB_EXCEPTIONS can be turned off with GCC and Clang alone.")
        endif()
    else()
        target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
        if(NOT APLOMB_EXCEPTIONS)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()
    endif()
endfunction()

# Builds everything in the calling directory and below with the given sanitizers, such as
# "address;undefined", compiling and linking alike. Any finding ends the program, so that a test
# that meets one fails.
function(aplomb_enable_sanitizers sanitizers)
    if(NOT sanitizers)
        return()
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR "APLOMB_SANITIZERS can be set with GCC and Clang alone.")
    endif()
    list(JOIN sanitizers "," joined)
    add_compile_options(-fsanitize=${joined} -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${joined})
    if("undefined" IN_LIST sanitizers)
        # The check of dynamic types needs the type information of every polymorphic class, and
        # Jolt is built without it.
        add_compile_options(-fno-sanitize=vptr)
    endif()
endfunction()
