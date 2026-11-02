# AGENTS.md — VanillaBTN

Native bring-up of *Harvest Moon: Back to Nature* (PS1, USA, `SLUS_011.15`) in **Rust**, with a modern renderer and audio. Formerly `hmbtn-decomp`.

The C code in `src/` is a **read-only reference** for game behavior. There is no matching/SHA-1 check; do not edit `src/` and do not try to build it. All new code is Rust. No Docker, no CI.

## Milestones

One branch and one PR per milestone. A milestone is any coherent unit of work (for example "Windows bringup" or "feature A"), not only the list below, which is the initial plan. Work on the current milestone only.

1. **Boot:** window opens, game code runs up to the title screen.
2. **Title to gameplay:** title, new game/load, farm scene playable.
3. **Modern renderer:** PGXP-style fixes, resolution scaling, filtering, backend switching.
4. **Audio:** SPU/XA accuracy, reverb, gapless streaming.
5. **Settings:** in-game settings menu (game's own UI style), config file.
6. **Platforms:** Windows first, then Linux, macOS, Android.

## Boot loop (milestone 1 and any "game won't run" work)

Run the game under `timeout`, take the **first** failure, fix its root cause, repeat. Don't fix several things at once.

Run contract: `cargo xtask run -- --frames N [--dump-frame K out.png]` prints the boot stage reached, frames run, and the failure reason (create it minimal if missing). After 3 failed attempts at the same failure, stop and report what you learned instead of piling on changes.

- **If the disc image is not reachable, stop and tell the user.** Do not work on unrelated things.
- Fix root causes. No stubs that just return 0, no hacks that hide the failure, no `todo!()` left on a path the game reaches. If something is stubbed on purpose, mark it `// STUB:` and list it in `STATUS.md`.
- Keep `STATUS.md` (max ~15 lines): current milestone, what works, what is stubbed, next failure. Update it at the end of every session/task, not only per PR (the agent has no memory between sessions).
- Once the game boots, save golden frames (title, one gameplay scene) plus a logical CRC (VRAM model / GPU packet stream). After renderer changes compare on the same backend with a small pixel tolerance — pixels differ across GPUs, the logical CRC must match exactly.

## Rules

1. **Don't break other features.** Before a PR: `cargo fmt --check`, `cargo clippy -- -D warnings`, `cargo test`, run the game on the host platform, compare golden frames if they exist. Fixing A must not break B.
2. Never commit game data, disc images, saves, or Sony SDK files. The disc path comes from an argument/env var, never hard-coded.
3. Don't guess behavior: derive it from the C reference or documented PS1 behavior. Since the C can't be built, there is no behavioral oracle; add unit tests for GTE and other math from documented behavior, and say honestly what was and wasn't verified.
4. No unrelated refactors or renames.
5. Don't copy code from other emulators or PGXP implementations (licenses vary: GPL, non-commercial, no-derivatives). Implement from documented behavior.

## Layout (Cargo workspace)

- `vbtn-core` — game logic ported from the C reference (no graphics/OS code)
- `vbtn-psx` — PSY-Q API reimplemented in Rust (GPU packets, GTE, SPU, CD, pad, memcard)
- `vbtn-gfx` — renderer + backends
- `vbtn-audio` — audio
- `vbtn-app` — entry point, in-game settings
- `xtask` — build/run/setup commands (`cargo xtask build|run|setup`)

`vbtn-core` must not depend on gfx/audio/OS crates. Platform-specific code lives in dedicated per-platform modules, not scattered `#[cfg]` through shared code.

## Port approach

Port function by function from the C reference. Reimplement PSY-Q at the library level (not hardware emulation): intercept GTE/GPU calls before PS1 integer rounding so the renderer gets full-precision vertices (PGXP-style: no wobble, perspective-correct textures). Milestones 1–2 use the plain integer (vanilla) path: do **not** build the high-precision path before the title screen renders, but keep the GTE/GPU API shaped so it can be added in milestone 3. Only GTE-transformed geometry benefits; check what the game actually draws that way before investing.

## PS1 limits

Keep the PS1 **addressing model** that game data relies on (VRAM coordinates, CLUT, texpage); remove **capacity and precision limits** on the renderer side: no 1 MB VRAM / 2 MB RAM cap, no ordering-table or primitive-count caps, no 15-bit color, no fixed low resolution, no integer vertex snapping. Use as much RAM/VRAM as the device has (high-res render targets, large caches). There are no high-res textures on the disc, so "better textures" means filtering/upscaling the existing ones, never new art.

- **GPU:** all rasterization, compositing, scaling and filtering run on the GPU.
- **CPU:** multiple cores for audio, asset decoding/streaming, and render-command building. Game logic stays deterministic on one thread.
- Measure before optimizing; keep frame pacing stable.

## New features

Build **only from what the game already has**: its assets, logic, data tables, and UI style. Do not invent your own design, art, sound, text style, layout, or rules. New menus and settings screens reuse the game's own windows, fonts, cursors, and sounds. If the game has nothing suitable, stop and ask the user.

Three kinds of features:

- **Mandatory** (crash/correctness fixes, required behavior): always on, no toggle.
- **Enhancements** (look, sound, feel: PGXP, scaling, filtering, widescreen, ...): **ON by default**, each switchable in settings, plus a master **Original** switch that restores vanilla behavior at once. An enhancement the device can't run turns itself off with an in-game notice. Enhancements must not change game logic or state.
- **Personal** (anything that changes gameplay/logic, e.g. a user-requested tweak): switchable in settings, **OFF by default**, only built when the user asks for it.

If unclear which kind, ask the user. Never remove or alter an existing game feature (for example, a creature or event) unless the user explicitly asks for that specific removal.

## Renderer

Selectable in the in-game settings (saved to a config file). Applying a change restarts the renderer/app — no live hot-swap of backends. On Windows "DirectX" means DX12 through `wgpu`.

- Windows: DirectX + Vulkan (first target)
- Linux: Vulkan + OpenGL
- macOS: Metal (+ Vulkan/GL only if needed)
- Android: Vulkan + OpenGL ES, with separate handling for **Adreno vs Mali** via a GPU-quirk table in `vbtn-gfx` (no scattered `if adreno`; a fix for one vendor must not change the other)

Prefer one abstraction (e.g. `wgpu`) over hand-written backends.

## Scripts

`cargo xtask` is the single implementation of build/run/setup. `scripts/` has thin `.sh`, `.cmd`, `.ps1` wrappers that only call it, so they can't drift. Fail on first error; pinned (SHA-256) downloads if anything is downloaded.

## Git

- **One PR per milestone** into `main`; never push to `main` directly and don't open extra PRs mid-milestone. Inside the milestone branch, commit per piece of work (each commit builds and does one thing); a milestone does not have to be one commit. Merge keeps those commits (no squash).
- Commits and PR titles: Conventional Commits in **English** (`feat:`, `fix:`, `chore:`, `refactor:`, `docs:`, `test:`, `perf:`, `build:`).
- PR description: goal, what changed, how verified, known limits.

## Style

Stable Rust, fmt/clippy clean, no `unwrap()` in runtime paths without a documented reason, `unsafe` only where required and commented, no per-frame allocations in render/audio paths.

## License

This repo's own code is **MIT OR Apache-2.0**: add `LICENSE-MIT` and `LICENSE-APACHE` (standard texts) and `license = "MIT OR Apache-2.0"` in every `Cargo.toml`. The reference C in `src/` was released as CC0 by its author; keep its notice, and keep any license file that ships with third-party tools (for example `tools/maspsx`). The game and the PSY-Q SDK belong to their owners and are never included. New dependencies must be MIT/Apache-compatible; no GPL or non-commercial code.
