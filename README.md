<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="branding/aplomb-logo-dark.svg">
    <img src="branding/aplomb-logo.svg" alt="aplomb" width="320">
  </picture>
</h1>

aplomb is an engine-independent active ragdoll library built on Jolt Physics. Its characters keep
their balance, take steps, brace for falls, protect themselves and react to the world in a
physically correct way.

It is at an early stage. Today `aplomb::core` holds a world that owns a Jolt physics system and
steps it at a fixed rate, hashes its whole physical state and records each step's messages and
state hash, passive ragdoll characters made from rigs, the checks of preconditions with their
replaceable handler, and the replaceable log sink. `aplomb::files` reads rigs from rig files and
scenarios from scenario files, reporting every problem in one with its line, column and JSON
pointer, and writes and reads replay files, whose numbers read back with the bits they were written
with; `assets/rigs/humanoid.json` is a 16-part humanoid rig. The scenario runner, `aplomb_runner`,
runs a scenario with no window at the step rate it is given, writes what it measured to a metrics
file, and fails when the scenario's pass criteria do, when its final state hash isn't its golden
hash, or, run twice, when the two runs' state hashes differ at any step. It records a run as a
replay, and plays a replay back, failing at the first step whose state hash differs from the
replay's; the scenarios in `tests/scenarios/` run at 60 Hz and 120 Hz with the tests. How the code
is written is in `CODING_STANDARDS.md`.

aplomb is under the MIT licence; see `LICENSE`. The libraries it ships with, and their licences,
are in `THIRD_PARTY_NOTICES.md`.

All rights to the logos in `branding/` are reserved. See `branding/README.md`.
