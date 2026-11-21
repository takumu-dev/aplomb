# The oldest compilers aplomb supports. The floors rise at each milestone and never fall, and
# configuring with anything older stops here, rather than somewhere deep in a standard header.

include_guard(GLOBAL)

# Stops the configuration if the C++ compiler with this CMake compiler id and version is one
# aplomb supports, but older than its floor. Any other compiler gets a warning.
function(aplomb_check_compiler_floor id version)
    if(id STREQUAL "GNU")
        set(floor 16)
        set(needed "GCC 16")
        set(found "GCC ${version}")
    elseif(id STREQUAL "Clang")
        set(floor 23)
        set(needed "Clang 23")
        set(found "Clang ${version}")
    elseif(id STREQUAL "AppleClang")
        # The Apple Clang of Xcode 27.2. CMake adds Apple's build number to its version, which tells
        # apart Xcodes whose Apple Clang has the same version.
        set(floor 21.0.0.21000401)
        set(needed "Apple Clang 21.0.0.21000401 (Xcode 27.2)")
        set(found "Apple Clang ${version}")
    elseif(id STREQUAL "MSVC")
        # CMake reports cl's own version, which runs five majors ahead of the toolset's.
        set(floor 19.52)
        set(needed "MSVC 14.52 (cl 19.52)")
        set(found "cl ${version}")
    else()
        # Nothing is known about other compilers, so they may try.
        message(WARNING
            "aplomb supports GCC, Clang, Apple Clang and MSVC; ${id} ${version} is untested, and "
            "the build may fail."
        )
        return()
    endif()

    if(version VERSION_LESS floor)
        message(FATAL_ERROR "aplomb needs ${needed} or newer; this is ${found}.")
    endif()
endfunction()
