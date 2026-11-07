# Native port build policy

## Scope and current status

This defines the intended Debug and Release behavior of the future VanillaBTN
native port. It is a policy and checklist, not an implemented build system.

The current PS1 decompilation remains separate. Its source, SDK, compiler pipeline,
executable, overlays, and disc outputs are not included in native Debug/Release.
Its existing commands and SHA-1 checks remain valid. No new Matching profile is
needed: the existing PS1 build already serves that purpose.

## Profile contract

| Concern | Native Debug | Native Release |
| --- | --- | --- |
| Goal | Inspect execution and diagnose development problems | Ship an optimized application |
| Optimization | Disable or minimize optimization for reliable stepping | Enable production optimization |
| Symbols | Usable source, function, and variable debug information | Separate symbols for diagnosis when supported |
| Diagnostics | Development logging, assertions, and useful checks | Development logging, assertions, and debug tools compiled out |
| Safety | Supported memory/runtime checks where practical | Required input validation and safe failure handling retained |
| Output | Separate Debug build and run directories | Separate Release build and package directories |

No native executable is produced by this documentation change.

## Compiler and build configuration

- Define Debug and Release explicitly when native targets are introduced. Reject
  unsupported configurations rather than silently mixing settings.
- Apply options to native targets only. Do not reuse PSY-Q/MIPS flags, PS1 include
  paths, absolute memory layouts, or SDK libraries as host build configuration.
- For MSVC, Debug should use the equivalents of `/Od` and `/Zi`, with matching
  debug runtime and linker settings. Release should use `/O2` and `NDEBUG`, with
  consistent release runtime settings. Verify the generated commands.
- For GCC/Clang, Debug should start with `-O0 -g` or a verified debugger-friendly
  alternative such as `-Og -g`. Release should start with `-O2 -DNDEBUG`. Choose
  exact flags after the native toolchain and dependencies are known.
- Add fast-math, LTO, aggressive optimization, and sanitizers only when supported
  and verified for the affected targets; they are not automatic requirements.
- Keep dependency configurations, runtime libraries, and output directories
  consistent with the selected profile, including multi-configuration generators.
- Provide tested build commands/presets when implemented. Do not present planned
  commands as if they already work in this repository.

## Logging, assertions, and errors

- Define a documented compile-time diagnostic interface for native code. Debug
  should provide severity, context, and a documented output sink. Bound or throttle
  repetitive logs, especially from frame and audio callbacks.
- Release must compile out development logging and debug assertions, including
  argument evaluation, diagnostic-only allocations, strings, and initialization.
  A runtime verbosity switch alone is insufficient.
- Never put required state changes, resource operations, or input validation in
  an assertion or diagnostic argument. Normal behavior must survive their removal.
- Exclude debug menus, overlays, inspection tools, and their startup hooks from
  Release. Do not classify functionality solely from its name.
- Preserve production error handling: missing assets or unavailable audio devices
  must still produce safe, understandable behavior. Explicitly distinguish any
  production error reporting from development logging and protect privacy.
- Sanitize logs, symbol/source paths, crash artifacts, and examples. Never expose
  personal home paths, secrets, or private environment values in shared outputs.
- Keep development tools and symbols out of the normal runtime package; retain a
  separate sanitized symbol artifact when crash diagnosis requires it.

## Verification and completion

The following checklist applies when native profiles are implemented. It remains
pending because this change establishes documentation and rules only.

- [ ] Build Debug and Release independently with the supported native compiler;
  inspect flags and dependency/runtime selection for each.
- [ ] Confirm incremental builds and profile switching do not reuse incompatible
  objects or dependencies, and that outputs are separated by profile.
- [ ] Launch both profiles and verify a responsive window, input, rendering, and
  clean shutdown for the functionality implemented at that stage.
- [ ] For implemented audio playback, verify audible output in both profiles;
  unit tests, initialization logs, and audio API success alone are insufficient.
- [ ] Demonstrate Debug logging, an intentional failed diagnostic check, and
  debugger breakpoints/stepping in the actual native application.
- [ ] Inspect Release objects/disassembly to confirm diagnostic sites and argument
  side effects are absent; verify that no debug UI or log sink starts at runtime.
- [ ] Exercise missing assets and unavailable devices without relying on Debug
  assertions for production safety.
- [ ] Re-run the separate PS1 matching build if shared build rules or PS1 files
  are touched; never change reference hashes to accommodate native work.
- [ ] Record sanitized commands, observed runtime results, and remaining limits
  in the task record or PR, then request developer review.

Apply checks to the current phase's acceptance criteria. Do not invent window or
audio features to complete this checklist: record unimplemented features as future
scope. If a required scenario cannot be exercised, leave validation pending and
state the missing prerequisite instead of claiming success.
