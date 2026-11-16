# Architecture — Native Runtime Target

> **Design intent, not implementation evidence.** Inspect the checkout before treating any crate, dependency, interface or test as existing. This document defines boundaries and fidelity constraints rather than ordering a large initial abstraction framework.

## Overall model

```text
+-------------------------------------+
| VanillaBTN Windows native executable|
+-------------------------------------+
        |            |          |
        v            v          v
  Game state +    Graphics    Host I/O
  fixed-step      renderer    audio/input/save
  simulation          |          |
        |             v          v
        +-------- Windows host CPU/GPU/OS
        |
        v
  Bounded game-resource readers  <---- user-provided original data

Original decompiled C / original disc = comparison reference,
not an executable runtime dependency of the final game.
```

**Build strategy:** introduce only the crates/modules needed by the **current** milestone. The diagram is a target dependency direction, not permission to scaffold unneeded engines, plugin systems or abstract APIs.

## Architecture decisions and authority

[`stack.md`](stack.md) is the **binding technology baseline**; [`workflow.md`](workflow.md) decides task ownership and approval gates. This architecture document describes interfaces and fidelity invariants only. Do not invent a game engine/ECS, async runtime, generic resource registry, non-`wgpu` graphics backend or fallback audio backend, or excessive crate workspace until a scoped task demonstrates the need and the developer authorizes any stack change.

## Subsystems and contracts

| Concern | Required invariant | Typical native responsibility |
| --- | --- | --- |
| Bootstrap | Start/exit without emulator or PS1 BIOS | Windows application lifecycle, event loop, fault handling. |
| Asset access | Never read beyond boundaries; no proprietary files in Git | Read-only indexing, validation, owned decoded data. |
| Game state | Original event order, RNG, time and interaction semantics | Explicit state ownership and deterministic updates. |
| Rendering | Original PS1 visible rules as reference | Native GPU textures/CLUT, primitives, draw ordering, blending and viewport. |
| Input | Original control semantics | Keyboard/gamepad input mapped to logical game actions. |
| Audio | Original sequencing/playback expectations | Native decode, mixing, timing and device output. |
| Persistence | Preserve data integrity and required compatibility | Native save/load with validation and atomic replacement when feasible. |
| Diagnostics | Debug-only logging; Release contains no application logging path | Compile-time conditional modules, bounded `VanillaBTN.log`. |

The renderer is **not** the simulation clock. Do not tie game time, RNG, NPC updates or collision to host refresh rate. Do not claim original game behavior without reference evidence or developer observation.

## Decompilation hazards to resolve explicitly

- PS1 fixed addresses and 32-bit data layouts are not safe assumptions for 64-bit native pointers.
- Overlay loading, SDK/BIOS calls, CD I/O, GPU ordering tables/CLUT, SPU/XA audio and memory card behavior require native equivalents.
- Decompiled C is a reference for constants and semantics, **not** automatically portable C that should be compiled into the Rust game.
- Untrusted or damaged assets and save files must be rejected safely, including in Release. `debug_assert!` is not a replacement for validation.
- `unsafe` and FFI require an explicit demonstrated need, narrow boundary, and documented invariants.

## Implement incrementally

1. R1: minimum real window and correct Debug/Release binary contracts.
2. R2–R3: only the asset types and GPU rules actually needed for representative startup scenes.
3. R4–R6: progressively transplant observable game logic while checking transitions against the reference.
4. R7–R8: audio, persistence, end-to-end original behavior and Windows acceptance.

**Technology choices are LOCKED by [`stack.md`](stack.md), not agent-selected.** Windows MSVC + `winit` + multi-backend `wgpu` (`dx12`, `vulkan`, `gles`/OpenGL, `metal`) + WGSL + `kira`/`cpal` + `gilrs` + `glam`, dedicated Rust PS1 asset/audio decoders, stdlib save I/O and Debug-only `tracing` are the approved baseline. Add only the dependencies a current approved task needs. Any replacement requires developer approval through [`workflow.md`](workflow.md).

## Invariants through all milestones

- No modifications to upstream `.github/workflows/`; no agent-run CI.
- No changes to decompiled C/reference bytes for convenience.
- No original-game enhancements during as-is bring-up.
- No licensed disc content or private saves in commits/build bundles.
- Debug and Release game states remain equivalent despite different diagnostics/optimization.
