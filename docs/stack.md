# Approved Technology Stack — VanillaBTN (Binding)

> **STATUS: APPROVED PROJECT BASELINE / POLICY, NOT AN IMPLEMENTATION CLAIM.**
> Applies to Windows native Rust **as-is** bring-up R0–R8. An agent MUST follow it, even if another crate seems more fashionable or convenient. Developer-only authority may change it. Read [`workflow.md`](workflow.md) for the approval procedure.

## 1. Decision matrix (no agent substitutions)

| Area | Locked decision | Scope / non-negotiable boundary |
| --- | --- | --- |
| Product and language | **Rust stable**, native Windows x86-64 **MSVC** (`x86_64-pc-windows-msvc`) | Game/runtime must not execute decompiled MIPS/PS1 BIOS/PSY-Q. No C++ rewrite or large C-FFI bridge. |
| Windows compiler tools | Visual Studio **Desktop development with C++** + Windows SDK/MSVC linker; Clang available as optional diagnostic tool | **MSVC** is the shipping target. Do not switch to GNU/Clang toolchains as the default. |
| Window, OS events, keyboard/mouse | **`winit`** | One native window/event loop; keyboard/mouse input via `winit`. No SDL2, GLFW, or alternative game-loop framework. |
| GPU abstraction | **`wgpu`** | Native host GPU across `wgpu`-supported backends (`dx12`, `vulkan`, `gles`/OpenGL, `metal` on Apple targets). Target **D3D12** by default on Windows and allow selecting or verifying alternate `wgpu` backends (`vulkan`, `gl`) cleanly. No hand-written DirectX/OpenGL/Vulkan renderer outside `wgpu` or replacing `wgpu` with SDL, `vulkano`, etc. (Note: `wgpu` 27 provides no separate native DX11 backend unless external ANGLE/GLES binaries are supplied.) |
| GPU shaders | **WGSL** | Compile and use native `wgpu` pipelines; preserve PS1 drawing rules (ordering table effects, CLUT/palettes, transparency, texture coordinates, viewport). No shader overhaul/enhancement. |
| Audio output / mixing | **`kira`** (its supported **`cpal`** audio device backend) | Use native Windows audio device through selected library. Reproduce cue/sequence behavior; keep game audio distinct from a platform mixer. |
| Original audio formats | **Small dedicated Rust format decoders/parsers, only for formats observed in the original data** | `kira` is the output/mixer, **not** presumed to decode PS1 SPU/ADPCM/XA/sequence assets. No automatic FFmpeg dependency or prerecorded replacement music. |
| Gamepad | **`gilrs`** | Map controller events into a single logical-action layer; keyboard via `winit`. Do not add a second controller framework. |
| Math | **`glam`** for graphics vectors/matrices | For gameplay timing/collision/RNG, retain reference integer or fixed-point semantics as needed; do not substitute float math where parity could change. |
| Original asset I/O | **Rust `std::fs` / `std::io` + project-specific bounded parsers** | Load legally provided disc/archive data read-only; no runtime PS1 CD emulation, no unpacked proprietary assets in Git. |
| Save/persistence | **`std::fs` + explicit native format adapter** | Preserve/validate reference save semantics; use atomic replace where feasible. **Do not introduce `serde`, `bincode`, SQLite, or change format just for convenience.** |
| Diagnostics | **`tracing` ecosystem, Debug-only**, including `tracing-subscriber` and optional `tracing-appender` if rotation needs it | Debug file `%LOCALAPPDATA%\VanillaBTN\VanillaBTN.log`; debug call sites and subsystem excluded via `#[cfg(debug_assertions)]` in Release; verify no logging code/path/output in Release. |
| Concurrency | **Rust standard library first**, only when required | No Tokio/async runtime, Rayon, ECS, message bus, plugin system or job scheduler without explicit approval. `wgpu` init futures may use the smallest justified synchronous bootstrap helper; this is not approval for an async runtime. |
| Test/build | Cargo, `rustfmt`, Clippy, Rust unit/integration tests; local Windows Debug **and** Release execution | Tests are local, not CI. Keep original hmbtn-decomp CI untouched. Docker only optionally for local original PS1 reference checks. |

**Windows controller note:** `gilrs` currently defaults to Windows Gaming Input (WGI), which may require a focused window to receive events. Validate actual controller behavior with the `winit` window active; a change to `gilrs` XInput features remains within the approved gamepad library but needs task-specific justification and local verification.

**Dependency discipline:** “Locked” means the approved choice **when that subsystem is implemented**, not permission to add every crate in R1. Add a crate only in the milestone that needs it; record the resolved version in `Cargo.lock` and inspect APIs/licenses/security compatibility at implementation time. No exact crate version is invented in this documentation starter. Treat `Cargo.lock` for the application as tracked after creation. A minor helper crate is not automatically approved: apply the change-control rule below.

## 2. Rendering and audio fidelity contracts

**Renderer:** `winit` owns the OS event loop; `wgpu` owns the device, surface, swapchain/presentation and GPU resources across `wgpu`-supported hardware backends (`dx12`, `vulkan`, `gles`/OpenGL, `metal`). Default to **D3D12** on Windows while supporting explicit backend selection (`WGPU_BACKEND`) and verifying rendering parity across active Windows hardware backends (`D3D12`, `Vulkan`, and `OpenGL`) starting at R3 before progressing to R4+. Extract original textures/palettes, geometry, clipping, ordering, blending and camera rules from evidence. Explicitly validate color conversion, alpha/PS1 semi-transparency, draw ordering and viewport against known reference scenes. Do **not** use shaders to approximate behavior without documenting mismatches. No HD post-process, widescreen, uncapped gameplay or arbitrary scaling upgrades during bring-up.

**Audio:** `kira` drives host mixing/device output; the project itself parses/decodes necessary original sound resources and follows original scheduling/rate/loop/cue behavior. Do not assume all PS1 music is a standard audio file. Compare event timing and audible behavior against reference observations with legally sourced assets. Audio mixer != game simulation clock. No new soundtrack, synthetic replacement, remastering or arbitrary codec pipeline.

**Simulation:** preserve observed PS1 game semantics (update order, discrete timing, RNG, fixed-point where applicable) independent from GPU present rate or audio callback. Avoid hardcoding "30 FPS" or "60 FPS" without evidence from source/reference. CPU/GPU/RAM native access does **not** imply deliberately maximizing hardware utilization.

## 3. What agents may and may not decide

**Allowed without approval:** local private structs/functions, scoped modules, tests, bug fixes within an already-approved task, choice of a small standard-library API, and the smallest implementation of a documented algorithm. Any work still needs a task ID and acceptance check.

**Approval required before implementing:** swapping any locked crate (or `wgpu` itself), adding an ECS/engine/game framework, adding an async runtime or heavyweight dependency, introducing FFI as an architectural shortcut, introducing a new save format, changing game behavior, adding a new non-wgpu renderer, modifying original decomp source or CI, and introducing a fallback audio backend. Changing dependency versions to resolve build problems is allowed only if compatible with the locked stack and current task; record the evidence and `Cargo.lock` diff.

**Proposed change protocol:** Stop the affected scope; document the verified blocker, smallest choices and trade-offs, proposed change, impact on fidelity/build/tests, and affected task ID in the **handoff to the developer**. Do not edit this stack policy, add the dependency, create a new milestone or silently implement the alternative. Only after explicit developer permission, update this file and relevant docs in one appropriately scoped commit.

If a locked component is unworkable, report **BLOCKED**, not "solved by switching libraries." If this pack is integrated into an existing checkout with previously approved architecture, **surface conflicts for developer reconciliation** rather than overwriting working decisions.
