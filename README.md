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

## Building

aplomb builds with CMake 4.4 or newer, from the presets in `CMakePresets.json`. Each takes the
dependencies from vcpkg, as `vcpkg.json` and the overlay ports in `ports/` give them, so set
`VCPKG_ROOT` to a clone of vcpkg, then configure, build and test with one of the workflow presets:

```bash
cmake --workflow --preset linux-gcc-release
```

| Platform | Compiler                                | Presets                                      |
| -------- | --------------------------------------- | -------------------------------------------- |
| Linux    | GCC 16 or newer                         | `linux-gcc-debug`, `linux-gcc-release`       |
| Linux    | Clang 23 or newer                       | `linux-clang-debug`, `linux-clang-release`   |
| Windows  | MSVC 14.52 or newer                     | `windows-msvc-debug`, `windows-msvc-release` |
| macOS    | the Apple Clang of Xcode 27.2, or newer | `macos-clang-debug`, `macos-clang-release`   |

Configuring with an older compiler stops, naming the version it needs. The MSVC presets run from a
developer command prompt for MSVC 14.52. The debug presets build with AddressSanitizer and
UndefinedBehaviorSanitizer, except MSVC's, and the Clang release presets build the library without
exceptions.
Linux is where aplomb is developed and watched in the sandbox. CI builds and tests it on Windows and
macOS too, where every scenario must end with the golden hash it ends with on Linux.

## Using aplomb in a project

A host links two static libraries: `aplomb::core`, the world and its characters, and
`aplomb::files`, which reads rig files. aplomb finds what they need with `find_package` alone, so
the host supplies Jolt 5.6.0 or newer and pedantjson 0.2.0 or newer, through vcpkg, another package
manager or builds of its own. The world's interface is made of Jolt's types, so aplomb compiles
against the definitions in Jolt's CMake package, and must be built against the Jolt the host links.
The same results on every platform, and the golden hashes, need a Jolt built with
`CROSS_PLATFORM_DETERMINISTIC` on, as the overlay port in `ports/joltphysics` builds it.

The tests build a host's project both of the ways below.

### As an installed package

Installing a build of aplomb gives the two libraries, their headers and a CMake package, along with
aplomb's licence and the third-party notices; the scenes, the runner and the sandbox stay behind.
Here it goes to `/opt/aplomb`:

```bash
cmake --install build/linux-gcc-release --prefix /opt/aplomb
```

With the prefix on its `CMAKE_PREFIX_PATH`, the host finds the package, which finds Jolt and
pedantjson in turn:

```cmake
find_package(aplomb 0.1 CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE aplomb::core aplomb::files)
```

Until 1.0, a minor version may break what the last one offered, so the package answers a request
for its own minor version alone.

### As a subproject

The host adds aplomb's source to its own build, which finds Jolt and pedantjson:

```cmake
add_subdirectory(external/aplomb)
target_link_libraries(my_game PRIVATE aplomb::core aplomb::files)
```

As a subproject, aplomb builds no tests and no sandbox, and installs nothing, unless the host
turns `APLOMB_BUILD_TESTS`, `APLOMB_BUILD_SANDBOX` or `APLOMB_INSTALL` on. Its compiler settings
apply to its own targets alone, and its warnings become errors only where the host sets
`CMAKE_COMPILE_WARNING_AS_ERROR`. A host that builds without exceptions turns `APLOMB_EXCEPTIONS`
off, with GCC or Clang, and the library builds without them too.

## Licence

aplomb is under the MIT licence; see `LICENSE`. The libraries it ships with, and their licences,
are in `THIRD_PARTY_NOTICES.md`.

All rights to the logos in `branding/` are reserved. See `branding/README.md`.
