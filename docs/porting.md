# Windows port direction

VanillaBTN's current target is a native Windows port of *Harvest Moon: Back to
Nature* that preserves the original experience. The repository does not yet
build or run a native Windows game. Its matching PS1 decompilation is the
behavioral and data-layout reference for that work.

## Fidelity rules

- Preserve the original gameplay rules, pacing, content, and audiovisual
  presentation in the default experience. Avoid balance changes, replacement
  assets, or quality-of-life features in the baseline port.
- Keep optional modifications separate and opt-in if mod support is added.
  Creative mods should not change the default game experience.
- Keep the PS1 reference build independently reproducible and byte matching.
  Native platform changes must not weaken its SHA-1 checks.
- Do not distribute the original disc data or proprietary SDK. Use a player-
  supplied copy of the game for any assets the port needs.

## Native build profiles

The future Windows target must have distinct `Debug` and `Release` builds.
`Debug` should prioritize short compile times and useful diagnostic symbols.
Logs, console prints, traces, assertions, and debug UI belong in that build.

`Release` should use optimizations suited to runtime performance. Developer
diagnostics must be excluded at compile time so they do not execute or add
runtime overhead. Neither profile may depend on side effects inside diagnostic
code. User-facing game messages and required error handling are gameplay or
product behavior, not developer diagnostics.

Once the native target exists, verify that both profiles build and run, that
Release produces no developer diagnostic output, and that optimization does
not change the original game's behavior or timing. The PS1 reference build
keeps its existing compiler settings and SHA-1 checks.

## Bringup sequence

1. Identify PS1 platform calls and shared game logic in the existing code;
   document boundaries before moving or adapting code.
2. Establish a minimal native Windows executable and game loop, with platform
   services isolated from game behavior. Verify that it starts and exits cleanly.
3. Bring up input, timing, rendering, audio, and disc-data access in small,
   independently verifiable steps against the PS1 reference.
4. Bring up scenes and save data incrementally. Compare visible behavior and
   state transitions against the original game before expanding coverage.

This is a sequence for future work, not a claim that these systems already
exist. The PS1 `check` target verifies byte identity of the reference build;
native-port checks will need to verify behavior and Windows execution separately.
