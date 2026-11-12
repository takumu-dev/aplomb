aplomb_add_tests(files
    SOURCES
        humanoid_rig_tests.cpp
        metrics_file_tests.cpp
        rig_file_tests.cpp
        scenario_file_tests.cpp
    LIBRARIES
        aplomb::files
        aplomb::scenes
)
# The rig files that ship with aplomb, which the tests read from the source tree.
target_compile_definitions(aplomb_files_tests PRIVATE
    APLOMB_RIGS_DIR="${PROJECT_SOURCE_DIR}/assets/rigs"
)
