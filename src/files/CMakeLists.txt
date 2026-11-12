# aplomb::files, which reads rig files into what the core takes, and the only part of aplomb that
# opens a file. It links pedantjson privately, so a host that builds its rigs in code, or reads them
# its own way, never carries a JSON reader. Its public headers are included as <aplomb/files/...>.
find_package(pedantjson CONFIG REQUIRED)

add_library(aplomb_files STATIC)
add_library(aplomb::files ALIAS aplomb_files)
target_sources(aplomb_files
    PRIVATE
        file_reading.cpp
        json_file.cpp
        rig_file.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/aplomb/files/file_error.hpp
        include/aplomb/files/file_problem.hpp
        include/aplomb/files/rig_file.hpp
)
target_compile_features(aplomb_files PUBLIC cxx_std_23)
target_link_libraries(aplomb_files
    PUBLIC aplomb::core
    PRIVATE pedantjson::pedantjson
)
set_target_properties(aplomb_files PROPERTIES EXPORT_NAME files)
aplomb_target_conventions(aplomb_files)
