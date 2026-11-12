# Coding standards

How code in aplomb is written. Where a tool can hold a rule, it does: `.clang-format`,
`.clang-tidy` and the compiler settings in `cmake/AplombConventions.cmake` are the record of those
rules. This document covers the rest, and says why. Anything it leaves open follows the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines); where it
departs from them, it says so.

## Language and build

- C++23, shipped as ordinary headers rather than C++20 modules, with no compiler extensions.
- The supported compilers are GCC and Clang, with libstdc++, on Linux, and MSVC on Windows. Their
  floors are GCC 16, Clang 23 and MSVC 14.52. The floors rise at each milestone and never fall, and
  configuring with anything older stops with a message naming the floor.
- CMake 4.4 or newer. Every configuration the project builds is a preset in `CMakePresets.json`,
  with a workflow preset, so that a developer and CI build alike: `cmake --workflow --preset
  linux-gcc-debug` configures, builds and runs the tests. The presets find vcpkg through
  `VCPKG_ROOT`.
- The debug presets build everything with AddressSanitizer and UndefinedBehaviorSanitizer, and
  `linux-clang-release` builds the library with exceptions disabled (`APLOMB_EXCEPTIONS` off).
- CI runs on CircleCI, from `.circleci/`. Its Linux jobs run in an image built from
  `.circleci/image/`, which holds the floor compilers: `linux-gcc-debug`, `linux-clang-release`,
  `linux-arm64-release` (the same preset on ARM64) and `lint`. Together they are the `ci` check.
- The build only finds its dependencies with `find_package`, and never downloads anything, so a host
  can supply them through any package manager or none. aplomb's own builds take them from the vcpkg
  manifest, with test-only ports in its `tests` feature.
- A dependency must be under one of these licences: Zlib, MIT, BSD-2-Clause, BSD-3-Clause, BSL-1.0,
  ISC, Apache-2.0 with or without the LLVM exception, CC0-1.0 or Unlicense. A host can then ship
  under any terms. Build tools are held to the list too, though nothing of them ships. Configuring
  with vcpkg checks every installed port against the list in `cmake/AplombLicenses.cmake`, and
  stops on a licence outside it, or on one the check can't decide until a person has read it and
  recorded it there in `APLOMB_REVIEWED_LICENSES`.
- `THIRD_PARTY_NOTICES.md` lists every port a host ships with aplomb, with its licence text. It is
  generated from vcpkg's metadata by `cmake --build --preset <preset> --target third-party-notices`
  and committed, and a test fails when it no longer matches.
- Jolt comes from the overlay port in `ports/joltphysics/`, which builds it deterministic across
  platforms, in single precision, and pins its release. These options change Jolt's ABI, and Jolt's
  CMake package carries the definitions that match them, so whatever links `Jolt::Jolt` compiles
  against exactly that build. Moving to another release of Jolt is a `build(deps)` commit of its
  own.

## Targets

- aplomb is a library, not a framework: a host owns `main` and its frame loop, and calls into
  aplomb.
- `aplomb::core`, in `src/core/`, links Jolt and the standard library and nothing else. It holds the
  world and everything that controls a character. Its parts stay directories inside it, not
  targets, since they run as one ordered step. Its public headers sit under
  `src/core/include/aplomb/core/`, and its private headers beside its sources.
- A target includes only the headers of what it links, since each public include tree comes only
  with its library. Jolt's types may appear in the core's public headers, since aplomb is built on
  Jolt alone.
- aplomb never touches Jolt's process-wide state: its allocator hooks, `Factory::sInstance`,
  `RegisterTypes()` and its trace and assertion hooks. The host sets them up before it creates a
  world, and a world asserts that it has. The host lends a world its job system and temporary
  allocator too, and a world never starts threads or allocates scratch memory of its own.
- Apart from the assertion handler and the log sink, all state lives in a world or its characters.

## Determinism

A world gives the same state from the same inputs on every platform, which golden hashes and
replays depend on. The core keeps to the rules Jolt's deterministic build keeps to:

- Floating point is precise: `-ffp-contract=off` with GCC and Clang, `/fp:precise` with MSVC, and
  never fast math. `aplomb_target_conventions` sets this on every target of the library.
- Trigonometric and other transcendental functions come from Jolt, such as `JPH::Sin`, `JPH::Cos`
  and `JPH::ATan2`, never from `<cmath>`, whose results differ between standard libraries.
  `std::sqrt`, which IEEE 754 rounds exactly, is allowed.
- No result depends on the order of an unordered container, or on the order of Jolt's broad-phase
  queries, narrow-phase results or listener callbacks.
- Randomness comes from a generator seeded through the world, never from the wall clock or a
  `std::random_device`.

## Naming and spelling

- `PascalCase` for types, concepts, type aliases and template parameters.
- `snake_case` for functions, variables, parameters, namespaces, enumerators and `constexpr`
  constants, with no `k` prefix.
- Private and protected data members end in an underscore (`state_`). A plain struct's fields are
  bare.
- Enumerations are `enum class`.
- Macros only where nothing else will do, named `APLOMB_UPPER_CASE`.
- Everything is in one namespace, `aplomb`, with internals in `aplomb::detail`.
- Prose is British: comments, documentation, the README and commit messages. Every identifier is
  American, file-format fields included, so a comment "the behaviour's centre of mass" sits above
  `behavior.center_of_mass()`. One rule for every identifier means no word needs a judgement call.
- Names from aplomb's domain use its terms, and never the words they replace:

  | Say | Not |
  |---|---|
  | host | client, application, user |
  | world | simulation, level |
  | scene | level, environment, map |
  | character | ragdoll, puppet, actor |
  | rig | body, skeleton, model |
  | rig file | body file |
  | part | body, bone, limb, segment |
  | root part | hips, root bone |
  | joint, the link between a part and its parent part | constraint, bone |
  | bone, a node of the host's animation skeleton | joint |
  | muscle | motor, drive, actuator |
  | strength | stiffness, gain, weight |
  | joint torque | motor torque, external torque |
  | assist | cheat, hand of god |
  | pose | skeleton pose, bone transforms |
  | animation pose | target pose, input pose |
  | physics pose | ragdoll pose, simulated pose, output pose |
  | sensor | probe, measurement |
  | centre of mass (`center_of_mass`) | centre of gravity, centroid |
  | support polygon | base of support, footprint |
  | capture point | extrapolated centre of mass, XCoM |
  | behaviour (`behavior`) | action, skill, state |
  | arbiter | mixer, blender, scheduler |
  | preset | profile, style, personality |
  | message | command, request |
  | event | callback, signal |
  | push | shove, impulse |
  | hit | shot, impact, damage |
  | scenario | test case, level |
  | state hash | checksum, fingerprint |
  | golden hash | reference hash, expected hash |
  | replay | recording, demo |

  Jolt's own names, such as `Body` and `Constraint`, keep their meaning where code speaks of Jolt's
  objects themselves.

## Units

- Lengths are in metres, masses in kilograms and time in seconds. Space is right-handed with Y up.
- Angles are radians in code. Files state them in degrees, which their loaders convert.
- A step rate is in steps a second.

## Files and formatting

- Files are `snake_case.hpp` and `snake_case.cpp`, with one main type per header, named after it.
  Headers use `#pragma once`.
- Public headers are included as `<aplomb/core/...>`, and a target's private headers as `"..."`.
- `clang-format` decides the layout, and groups includes as: the file's own header, this project's
  headers, `<Jolt/Jolt.h>`, which Jolt requires before any other of its headers, other third-party
  headers, then the standard library. Build the `format` target to apply it, and `check-format` to
  check it.

## Errors and assertions

- A failure that correct code can meet, such as a bad rig file or a step rate read from a
  configuration, is returned as a value: the function returns `[[nodiscard]] std::expected<T, E>`.
  This departs from the Core Guidelines' E.2.
- Constructors never fail. Anything that can fail to come into being is made by a factory function,
  as a world is made by `World::create`.
- Each failure domain has an error type of its own, which carries an `enum class` code for programs
  to branch on, a message built where the failure happens, and a `std::formatter`. There is no
  library-wide error type, so that a caller can tell which codes a function returns.
- A load of a file reports every problem it finds, in phases, so that a hand-written file doesn't
  take one load per mistake.
- A broken precondition is a bug, not an error. `APLOMB_ASSERT(condition, message)` checks the cheap
  preconditions of the public interface in every build, release included.
  `APLOMB_DEBUG_ASSERT(condition, message)` checks internal invariants and expensive conditions
  where `NDEBUG` isn't defined. The message says what must hold, such as "a world is given a job
  system".
- A failed check is logged as an error, then handed to the assertion handler, which aborts unless a
  host has replaced it. Every other diagnostic aplomb doesn't return goes to the log sink. The
  handler and the sink are process-wide hooks, set once at start-up.
- No `throw`, `try` or `catch`, in the library or in its tests (bar the one test that a throw fails
  to compile without exceptions), since many hosts build with
  exceptions disabled. The library must build that way too, and stays exception-neutral: RAII
  throughout, so that an exception from the standard library or from host code that aplomb calls
  unwinds cleanly. A standard-library failure such as
  `std::bad_alloc` isn't caught.
- `noexcept` goes on moves, `swap`, and functions that have no precondition and can't fail. It never
  goes on a function with a precondition (the Lakos rule), so that a throwing handler stays
  possible.

## Ownership and types

- A plain struct that a host fills in with a designated initialiser, such as `WorldSettings`, gives
  each member an initialiser, `{}` for a class, so that any member may be left out without GCC's
  `-Wmissing-field-initializers`.
- Values first, and `std::unique_ptr` for a single owner. No `std::shared_ptr` without a comment
  saying why, and no bare `new` or `delete`, except where Jolt's interface takes a raw pointer it
  then owns, such as a `Shape` it counts references to.
- `T*` and `T&` never own. `T*` appears only where null means something, or where Jolt's interface
  takes one.
- Parts, joints and characters are named by typed IDs, each wrapping a `std::uint32_t` index into its
  table. "No object" is an empty `std::optional`, never a sentinel value.
- Counts and container indices are `std::size_t`, departing from the Core Guidelines' ES.107. Signed
  types are for arithmetic that can go negative.

## Warnings and clang-tidy

- The compiler warnings in `cmake/AplombConventions.cmake` are errors in every preset, through
  `CMAKE_COMPILE_WARNING_AS_ERROR`. Give a target of the library its settings with
  `aplomb_target_conventions`, and a test target with `aplomb_target_warnings`. The settings are
  private to each target, so none of them reaches a host's build. Third-party headers come in as
  system includes.
- The code passes clang-tidy with the groups in `.clang-tidy`, every warning an error. The `lint`
  workflow preset builds every source through clang-tidy, with the sanitizers on so that their
  tests are built and checked too, then checks the formatting. A finding that has to stand is
  suppressed with `NOLINT(<check>): <reason>` or `NOLINTNEXTLINE(<check>): <reason>`,
  never a bare `NOLINT`. Keep the reason on that line: `NOLINTNEXTLINE` covers only the line after
  its own, and any longer explanation goes in a comment above it. Jolt's interfaces are named in
  its own style, so overrides of its virtual functions keep Jolt's names.

## Documentation

- Every declaration in a public header has a `///` Doxygen comment, those in `detail` included, and
  each public header begins with a `\file` comment. Clang's `-Wdocumentation` checks each comment
  against its declaration.
- A comment says why, not what.

## Tests

- Tests come first: a test that fails, then the code that passes it. A new test that passes at once
  is checked by breaking the code it covers, since a test that can't fail proves nothing.
- A test exercises a public interface only, and survives any refactoring that keeps the behaviour.
  Its name is a sentence saying what holds, such as "a world steps at the step rate it is given". A
  test case's name must not begin with `-`, which Catch2 would read as one of its options.
- `tests/` mirrors `src/`, `tests/build/` holds tests of the build itself, named `build:`, and
  `tests/support/` holds what the tests share: Jolt's process-wide
  setup as a host does it, the job system and temporary allocator a host lends a world, with a fixed
  number of threads, and a log sink that records what it receives. No test opens a window.
- Each test's name begins with the target it tests, matching its Catch2 tag (`[core]`). Each kind of
  test has a helper in `tests/CMakeLists.txt`:
  - `aplomb_add_tests` adds a target's Catch2 test cases, each registered with CTest.
  - `aplomb_add_program_test` adds a program whose ending or output is the behaviour, which
    `tests/support/check_program.cmake` checks: an abort, an exit status, or what reaches stdout
    and stderr.
  - `aplomb_add_precondition_test` adds a program that breaks one precondition, which CTest expects
    to fail. It fails through a replaced handler rather than aborting, since CTest counts an abort
    as a crash even then, and only when the check that fails has the message the test names, so
    that no other check passes it. These are used sparingly.
  - `aplomb_add_compile_failure_test` adds a source built with the library's settings that must
    fail to compile, and for the reason the test names.
  - `aplomb_add_script_test` adds a CMake script that tests the build's own CMake code, such as
    the licence check on the fake vcpkg trees of `tests/build/licenses/`, through
    `tests/support/check_program.cmake` too.
- The core's tests build their rigs and scenes in C++, and never read a file.

## Commits and branches

- Commit subjects follow Conventional Commits, `type(scope)!: description`, with the type one of
  `build`, `chore`, `ci`, `docs`, `feat`, `fix`, `perf`, `refactor`, `revert`, `style` or `test`.
  The scope and the `!` of a breaking change are optional. A message describes the change and why.
- Branches follow git flow. `main` holds released versions only, and each merge into it is tagged
  with its version. `develop` is where work comes together. A change is made on `feature/<slug>`
  from `develop`, its slug naming the change; a release on `release/<version>`; and a fix to a
  release on `hotfix/<version>` from `main`. Every merge is `--no-ff`.
- Every branch gets a pull request, which must pass the `ci` check before the branch is finished
  with `git flow`.
