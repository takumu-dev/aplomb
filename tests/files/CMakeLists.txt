aplomb_add_tests(files
    SOURCES
        humanoid_rig_tests.cpp
        metrics_file_tests.cpp
        replay_file_tests.cpp
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

aplomb_add_precondition_test(files
    "writing a metrics file whose scenario name isn't UTF-8 fires its precondition"
    SOURCE programs/metrics_file_with_non_utf8_scenario.cpp
    MESSAGE "a metrics file's scenario name is UTF-8"
    LIBRARIES aplomb::files
)
