# The scenarios, each run at 60 Hz and at 120 Hz.
aplomb_add_scenario_test(drop_to_rest)
# Two runs of a character dropped from 1 m, which tumbles before it comes to rest, give the same
# state hash at every step.
aplomb_add_scenario_test(two_runs_agree DETERMINISM)
