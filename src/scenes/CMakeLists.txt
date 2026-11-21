# aplomb::scenes, scenes for a world's characters to stand on, built in code and found by name. It
# links the core for its object layers, and the library a host ships doesn't depend on it.
add_library(aplomb_scenes STATIC)
add_library(aplomb::scenes ALIAS aplomb_scenes)
target_sources(aplomb_scenes
    PRIVATE
        build_scene.cpp
        flat_ground.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/aplomb/scenes/build_scene.hpp
        include/aplomb/scenes/flat_ground.hpp
)
target_compile_features(aplomb_scenes PUBLIC cxx_std_23)
target_link_libraries(aplomb_scenes PUBLIC aplomb::core)
set_target_properties(aplomb_scenes PROPERTIES EXPORT_NAME scenes)
aplomb_target_conventions(aplomb_scenes)
