aplomb_add_tests(core
    SOURCES
        assertion_tests.cpp
        log_sink_tests.cpp
        world_tests.cpp
    LIBRARIES
        aplomb::core
)

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
