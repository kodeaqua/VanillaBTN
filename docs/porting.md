# Windows port direction

VanillaBTN's current target is a native Windows port of *Harvest Moon: Back to
Nature* that preserves the original experience. The repository builds a native
x64 Windows executable that runs the original game code, startup, and scene dispatcher. Its matching PS1
decompilation is the behavioral and data-layout reference for that work.

## Fidelity rules

- Bring up the existing game code through native platform and SDK interfaces.
  This is not a reconstruction of scenes, code, or gameplay. Do not implement
  replacement scene flows, dialogs, menus, or game rules to imitate the game.
  Limit original-code changes to compatibility needs and preserve behavior.
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

The Windows target has distinct `Debug` and `Release` builds.
`Debug` should prioritize short compile times and useful diagnostic symbols.
Logs, console prints, traces, assertions, and debug UI belong in that build.

`Release` should use optimizations suited to runtime performance. Developer
diagnostics must be excluded at compile time so they do not execute or add
runtime overhead. Neither profile may depend on side effects inside diagnostic
code. User-facing game messages and required error handling are gameplay or
product behavior, not developer diagnostics.

Agents verify that both profiles compile and pass code-level or synthetic
checks, including checks that Release excludes developer diagnostics. The
developer performs actual runtime and gameplay testing to judge behavior,
timing, visuals, and audio. The PS1 reference build
keeps its existing compiler settings and SHA-1 checks.

The staged implementation plan and stack decisions are in
[roadmap.md](roadmap.md). Milestones 0 through 5 are implemented with code-level checks,
and Milestone 6 has release packaging and runtime bringup in progress. The PS1 `check`
target verifies byte identity of the reference; the developer validates actual Windows
gameplay and runtime behavior.
