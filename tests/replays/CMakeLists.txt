# Sessions recorded in the sandbox, each played back by the runner, so that the sandbox and the
# runner are shown to step alike. A change to the rig file a replay names, or to how the world
# steps, means recording it afresh.

# Three humanoids dropped onto flat_ground at 60 Hz, each pushed in the chest in its first steps,
# one then pushed again in the head, falling and coming to rest over 460 steps.
aplomb_add_recorded_replay_test(sandbox_session RIG "${PROJECT_SOURCE_DIR}/assets/rigs/humanoid.json")
