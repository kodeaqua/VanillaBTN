# Architecture decisions

## ADR-001: Isolate the Phase 0 build boundary

Date: 2026-10-25. Status: accepted, following AGENTS.md defaults.

Context: The matching project installs a PSX toolchain before project creation.
The native port must work independently and must not require proprietary SDK input.

Decision: Keep the native CMake project and its presets in `port/`. Begin with
a C11 fixed-width type/endian test. Preserve all matching source and link rules.
Prepare matching dependencies using verified archives on Linux, with a local
owner-supplied SDK. Port checks run without network or game assets.

Consequences: Run port presets from `port/`. A foundation build does not imply
game boot. Renderer selection, overlay namespacing, asset layout changes, Android
minimum SDK and native host-tool version pins remain unresolved. No enhancement
is enabled. Workflow/CI work is deferred by owner instruction.

## ADR-002: Bring up Windows first

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The original roadmap starts native bring-up on Linux. The owner asked
to prioritize Windows during Phase 0.

Decision: Bring up Windows x86-64 first, using project-local checksum-pinned
LLVM-mingw/UCRT, CMake and Ninja. Windows port builds must not depend on WSL,
a PSX SDK, or the matching toolchain. Retain the other desktop foundation
presets for later validation. Matching validation retains its Linux requirement.

Consequences: The first game boot target is now Windows. Android is deferred.
No renderer or runtime architecture decision changes. Release links use static
compiler runtime dependencies and the Windows system UCRT; system DLL imports
remain expected.

## ADR-003: Validate locally and defer CI

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner explicitly excluded workflows and CI from the current work.

Decision: Leave existing workflow files untouched; add no CI provisioning or
jobs. Validate the Windows foundation locally and document actual results.

Consequences: Roadmap and completion requirements for a CI skeleton are deferred.
Existing upstream workflow behavior is not validated or endorsed by this work.

## ADR-004: Exclude WSL from Windows port work

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner does not want WSL used for this work.

Decision: Build and validate the port with native Windows tools. Do not install,
invoke, or require WSL. Defer matching validation to a separate Linux host when
available, preserving the requirement to report an unverified matching baseline.

Consequences: An unavailable Linux matching toolchain does not block Windows
port progress. Matching source protection and honest validation reports remain
required. No claim of a successful matching check follows from Windows tests.
