# aplomb::core, a static library that links Jolt and the standard library alone. Its public headers
# are included as <aplomb/core/...>.
find_package(Jolt CONFIG REQUIRED)

add_library(aplomb_core STATIC)
add_library(aplomb::core ALIAS aplomb_core)
target_sources(aplomb_core
    PRIVATE
        assertion.cpp
        log_sink.cpp
        world.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/aplomb/core/assertion.hpp
        include/aplomb/core/log_sink.hpp
        include/aplomb/core/object_layers.hpp
        include/aplomb/core/world.hpp
        include/aplomb/core/world_error.hpp
        include/aplomb/core/world_settings.hpp
)
target_compile_features(aplomb_core PUBLIC cxx_std_23)
# Public, since the world's interface is made of Jolt's types, and a host must compile against the
# same Jolt build: its package carries the definitions that build was made with.
target_link_libraries(aplomb_core PUBLIC Jolt::Jolt)
set_target_properties(aplomb_core PROPERTIES EXPORT_NAME core)
aplomb_target_conventions(aplomb_core)
