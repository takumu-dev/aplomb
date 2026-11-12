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
steps it at a fixed rate, the checks of preconditions with their replaceable handler, and the
replaceable log sink. How the code is written is in `CODING_STANDARDS.md`.

aplomb is under the MIT licence; see `LICENSE`. The libraries it ships with, and their licences,
are in `THIRD_PARTY_NOTICES.md`.

All rights to the logos in `branding/` are reserved. See `branding/README.md`.
