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
state hash, or that attaches to a host's own physics system and leaves stepping it to the host,
passive ragdoll characters made from rigs, the checks of preconditions with their
replaceable handler, and the replaceable log sink. `aplomb::files` reads rigs from rig files and
scenarios from scenario files, reporting every problem in one with its line, column and JSON
pointer, and writes and reads replay files, whose numbers read back with the bits they were written
with; `assets/rigs/humanoid.json` is a 16-part humanoid rig. The scenario runner, `aplomb_runner`,
runs a scenario with no window at the step rate it is given, writes what it measured to a metrics
file, and fails when the scenario's pass criteria do, when its final state hash isn't its golden
hash, or, run twice, when the two runs' state hashes differ at any step. It records a run as a
replay, and plays a replay back, failing at the first step whose state hash differs from the
replay's; the scenarios in `tests/scenarios/` run at 60 Hz and 120 Hz with the tests, and the
sessions recorded in the sandbox in `tests/replays/` play back with them.

The sandbox, `aplomb_sandbox`, is a window onto a world on flat ground. It loads a rig file, the
humanoid rig by default or the one its command line names, adds characters made from it and removes
them, and lets you watch them fall from any side: drag with the right mouse button to orbit, with
the middle button or shift and the right to pan, and scroll to zoom. Drag from a part with the left
button and let go to push it that way, harder the further the drag. Its panel pauses and resumes
stepping (space), takes one step at a time while paused (the right arrow), changes the speed and how
hard a drag pushes, and turns on and off the debug layers that show the characters' shapes, joints,
joint limits, contacts and centres of mass. It lists every problem in a rig file with its line and
column, and its rig inspector lists a character's parts and joints with their settings. It records a
session as a replay file, starting it afresh with its characters made from the loaded rig, which the
runner plays back to the same state hashes, and plays a replay back itself, saying at which step, if
any, its state hash first differed from the replay's. It draws with raylib on OpenGL 3.3 core, which
on Linux needs the headers of X11 and OpenGL to build, such as Debian's `libgl-dev`, `libx11-dev`,
`libxcursor-dev`, `libxi-dev`, `libxinerama-dev` and `libxrandr-dev`. It is built by every preset,
unless `APLOMB_BUILD_SANDBOX` is off. How the code is written is in `CODING_STANDARDS.md`.

aplomb is under the MIT licence; see `LICENSE`. The libraries it ships with, and their licences,
are in `THIRD_PARTY_NOTICES.md`.

All rights to the logos in `branding/` are reserved. See `branding/README.md`.
