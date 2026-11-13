aplomb_add_tests(core
    SOURCES
        assertion_tests.cpp
        character_error_tests.cpp
        character_tests.cpp
        log_sink_tests.cpp
        rig_problem_tests.cpp
        version_tests.cpp
        world_tests.cpp
    LIBRARIES
        aplomb::core
        aplomb::scenes
)
target_compile_definitions(aplomb_core_tests PRIVATE APLOMB_PROJECT_VERSION="${PROJECT_VERSION}")

aplomb_add_program_test(core "the default log sink writes each message to stderr with its severity"
    SOURCE programs/default_log_sink.cpp
    LIBRARIES aplomb::core
    EXPECTED_STDERR programs/default_log_sink.stderr
)
find_package(Threads REQUIRED)
aplomb_add_program_test(core
    "lines logged to the default sink from several threads at once arrive whole"
    SOURCE programs/concurrent_default_log_sink.cpp
    LIBRARIES aplomb::core Threads::Threads
    ARGUMENTS "${CMAKE_CURRENT_BINARY_DIR}/concurrent_default_log_sink.stderr"
)
aplomb_add_program_test(core "a null log sink restores the default"
    SOURCE programs/restored_log_sink.cpp
    LIBRARIES aplomb::core
    EXPECTED_STDERR programs/restored_log_sink.stderr
)
aplomb_add_program_test(core "a failed assertion is logged as an error and then passed to the handler"
    SOURCE programs/replaced_assertion_handler.cpp
    LIBRARIES aplomb::core
)
aplomb_add_program_test(core "a failed assertion is reported on stderr and aborts by default"
    SOURCE programs/default_assertion_handler.cpp
    LIBRARIES aplomb::core
    EXPECTED_RESULT abort
    STDERR_MATCHES "^error: .*default_assertion_handler\\.cpp:[0-9]+: assertion failed: a square has four sides \\(sides == 4\\).$"
)
aplomb_add_program_test(core "aplomb aborts when a replaced assertion handler returns"
    SOURCE programs/returning_assertion_handler.cpp
    LIBRARIES aplomb::core
    EXPECTED_RESULT abort
)
aplomb_add_program_test(core "a failed debug assertion fires only outside release builds"
    SOURCE programs/failed_debug_assertion.cpp
    LIBRARIES aplomb::core
)

aplomb_add_precondition_test(core "creating a world before Jolt's factory fires its precondition"
    SOURCE programs/world_before_factory.cpp
    MESSAGE "Jolt's factory exists before a world is created"
)
aplomb_add_precondition_test(core
    "creating a world before Jolt's types are registered fires its precondition"
    SOURCE programs/world_before_types.cpp
    MESSAGE "Jolt's types are registered before a world is created"
)
aplomb_add_precondition_test(core "creating a world without a job system fires its precondition"
    SOURCE programs/world_without_job_system.cpp
    MESSAGE "a world is given a job system"
)
aplomb_add_precondition_test(core
    "creating a world without a temporary allocator fires its precondition"
    SOURCE programs/world_without_temp_allocator.cpp
    MESSAGE "a world is given a temporary allocator"
)
aplomb_add_precondition_test(core "stepping a moved-from world fires its precondition"
    SOURCE programs/step_moved_from_world.cpp
    MESSAGE "the world hasn't been moved from"
)
aplomb_add_precondition_test(core
    "adding a character whose root rotation isn't a unit quaternion fires its precondition"
    SOURCE programs/character_with_non_unit_root_rotation.cpp
    MESSAGE "a character's root rotation is a unit quaternion"
)
aplomb_add_precondition_test(core "reaching a removed character fires its precondition"
    SOURCE programs/removed_character.cpp
    MESSAGE "the character is one of the world's characters"
)
aplomb_add_precondition_test(core
    "pushing a part the character's rig doesn't have fires its precondition"
    SOURCE programs/push_to_unknown_part.cpp
    MESSAGE "the part is one of the character's parts"
)
aplomb_add_precondition_test(core
    "stepping an attached world at a rate other than the world's fires its precondition"
    SOURCE programs/attached_world_stepped_at_another_rate.cpp
    MESSAGE "the host steps its physics system once by the world's step duration"
)
aplomb_add_precondition_test(core
    "stepping a host's physics system outside an attached world's step fires its precondition"
    SOURCE programs/host_update_outside_attached_world_step.cpp
    MESSAGE "the host steps its physics system only between before_physics_step() and after_physics_step()"
)
aplomb_add_precondition_test(core
    "creating an attached world without an object layer for parts fires its precondition"
    SOURCE programs/attached_world_without_part_layer.cpp
    MESSAGE "an attached world is given an object layer for its characters' parts"
)
aplomb_add_precondition_test(core "hashing an attached world's state fires its precondition"
    SOURCE programs/state_hash_of_attached_world.cpp
    MESSAGE "the world owns its physics system"
)
