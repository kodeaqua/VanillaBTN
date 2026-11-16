# VanillaBTN

**Windows-first, Rust-native, as-is bring-up of *Harvest Moon: Back to Nature*.**

VanillaBTN aims to preserve the original PlayStation game's **observable behavior** while replacing PlayStation runtime dependencies with a native Windows executable. This includes the game loop, rendering, input, asset loading, audio, and persistence. Direct use of host CPU/GPU/memory does **not** imply 100% hardware utilization; efficiency and correctness are the goals.

## Current scope

- **Active phase:** native Windows bring-up **as-is** (R0–R8).
- **Non-goals:** widescreen, high-FPS gameplay changes, new content, modified doors/animations, HD textures, modding, quality-of-life improvements, or other enhancements.
- **Reference:** [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp), a matching decompilation of the USA PS1 release. The decompiled C code and original build/CI are references, **not** the new Windows runtime.
- **Implementation:** Rust on Windows using the MSVC toolchain; **locked stack**: `winit` (window/events), multi-backend `wgpu` (`dx12`, `vulkan`, `gles`/OpenGL, `metal`) + WGSL (GPU), `kira`/`cpal` (audio), `gilrs` (gamepad), `glam` (graphics math), dedicated native asset/audio parsers, and Debug-only `tracing`. See [`docs/stack.md`](docs/stack.md).
- **Assets:** users supply legally obtained game data. No game disc image, extracted proprietary assets, proprietary SDK binaries, or personal saves belong in this repository.

## Read these in order

1. [`AGENTS.md`](AGENTS.md) — mandatory agent and contributor rules; Git/PR/versioning requirements.
2. [`docs/stack.md`](docs/stack.md) — mandatory renderer/audio/input/tooling decisions and their change-control gate.
3. [`docs/workflow.md`](docs/workflow.md) — exact hierarchy and workflow: roadmap → milestone → task → subtask → acceptance.
4. [`docs/roadmap.md`](docs/roadmap.md) — stable R0–R8 goals, prerequisite order and exit gates.
5. [`TODO.md`](TODO.md) — only actionable task queue, stable IDs, current status and evidence.
6. [`docs/architecture.md`](docs/architecture.md) — subsystem boundaries and fidelity invariants.
7. [`docs/build.md`](docs/build.md) — Windows toolchain, Debug/Release behavior, local commands.
8. [`docs/validation.md`](docs/validation.md) — parity checks and local acceptance protocol.

The upstream `README.md`, its Dockerfile, CMake build, original C code, and `.github/workflows/` must remain intact unless the developer explicitly authorizes a change.

## Working on the project

- Inspect existing code, Git state, documents, and version source **before** assuming the repository matches this documentation starter pack.
- Preserve pre-existing milestones and progress if merging this pack into a repository that already has `TODO.md` or `docs/roadmap.md`; merge selectively rather than overwriting them. **Roadmap is stable gates; TODO is stable task IDs + active evidence; agents may not freely rewrite either.**
- Work on the developer-agreed feature branch in small Conventional Commits; follow the project-specific `MAJOR.MINOR.PATCH` policy in `AGENTS.md`.
- Run checks **locally**. Do not create, modify, or manually invoke CI. Do not open a PR or merge before requested by the developer.
- Never declare a milestone complete without its acceptance evidence. Windows host runs that have not occurred remain **unverified**.

## Project status

This package is **documentation only**. It does not create a Cargo workspace, implement a renderer, generate game assets, modify upstream code, or demonstrate a successful Windows build. Reconcile each proposed TODO item against the real checkout before marking anything complete.
