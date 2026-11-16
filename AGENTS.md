# AGENTS.md — VanillaBTN

> Mandatory engineering instructions for any AI agent or contributor working on VanillaBTN.
> Read this file **before** inspecting, planning, editing, testing, committing, or preparing a PR.
> When rules conflict: the developer's explicit current instruction wins, followed by this file,
> then the approved stack/workflow policy, then the roadmap/TODO and existing conventions. Never silently resolve a meaningful conflict.

## 1. Project identity and vision

**Name:** VanillaBTN  
**Description:** A faithful, Windows-first, natively executable Rust reimplementation of *Harvest Moon: Back to Nature*, informed by the matching PlayStation decompilation at <https://github.com/coalslav/hmbtn-decomp>.

**Vision:** Preserve the original game's observable behavior while replacing PlayStation-specific runtime dependencies with efficient, maintainable host-native systems. Native CPU execution, host GPU rendering, host memory, audio, input, and storage must work without a PS1 emulator, BIOS, MIPS interpreter, or PlayStation hardware at runtime. Prioritize correctness, reproducibility, and incremental verification over speed of rewriting or speculative improvements.

**CURRENT PHASE: Windows native bring-up, AS-IS.** The objective is original gameplay/visual/audio/save behavior on Windows, **not** new mechanics, widescreen, higher FPS, HD textures, rewritten UX, new content, modding, or other enhancements. Preserve original timing and presentation where applicable. "Full host hardware" means using native APIs efficiently; it does **not** mean forcing CPU/GPU/RAM utilization to 100%.

Original game data is never assumed to be redistributable. Users must supply their own legally obtained game data when required. The decompilation repository's license does not grant rights to third-party game assets or proprietary SDK components.

## 2. Non-negotiable operating rules

1. **Never hallucinate.** Do not invent implementation status, file paths, APIs, functions, commits, test results, performance figures, game behavior, or successful verification. Inspect the source and evidence. Distinguish **verified**, **inferred**, **unverified**, and **blocked** explicitly.
2. **Think before coding.** Identify the requested outcome, inspect relevant code, name assumptions, compare viable approaches, and select the simplest correct solution. If a decision changes scope, behavior, compatibility, or architecture and cannot be resolved from evidence, ask the developer; otherwise make conservative, reversible progress.
3. **Simplicity first.** Implement only what the current milestone needs. No speculative abstractions, generalized frameworks, unnecessary configuration, preemptive fallback chains, dependency sprawl, or unrelated modernization.
4. **Surgical changes.** Make the smallest change that satisfies a verifiable goal. Do not opportunistically reformat, refactor, rename, delete, or "clean up" unrelated code. Match existing code conventions. Remove only dead code introduced by your own changes.
5. **Correctness over appearance.** A working window, successful compile, or green unit test does not by itself prove gameplay parity. Never mark a gameplay milestone complete on that basis alone.
6. **No hidden side effects.** Do not silently change versions, branches, Git history, CI, developer settings, game assets, save formats, or established project architecture.
7. **No destructive shortcuts.** Never discard user changes, overwrite unknown local modifications, reset the repository, rewrite published history, force-push, or delete a working implementation without explicit authorization and a verified replacement.
8. **No fabricated progress.** Mark a task completed only after its stated acceptance criteria have been demonstrated. Report blockers and partial work as such.
9. **Preserve the PS1 reference.** Treat upstream `src/`, `sdk/`, `disc/`, `cmake/`, and original build tooling as reference/baseline unless the developer explicitly authorizes changing them. In particular, do not change the original decompiled `src/` bytes during Rust native bring-up. Put native implementation in separately identified code paths.
10. **No enhancements during bring-up.** Even if a change looks desirable (door animation, speedups, UI polish, widescreen, unlocked FPS), record it as a future idea rather than implementing it.

The above incorporates the principles of the [Andrej Karpathy coding guidelines](https://raw.githubusercontent.com/multica-ai/andrej-karpathy-skills/refs/heads/main/CLAUDE.md): think before coding, keep it simple, edit surgically, and work toward measurable, verified outcomes. The project-specific requirements here take precedence where necessary.

## 3. Source of truth and documentation discipline

At the beginning of **every work session**, inspect the actual repository (do not assume the files below exist) in this order:

1. `AGENTS.md` and any more-specific instructions in the affected directories.
2. `git status`, current branch, recent commits, and the tracked version source.
3. `README.md`, `VANILLABTN.md`, **`docs/stack.md`, `docs/workflow.md`**, `docs/roadmap.md`, `TODO.md`, `docs/architecture.md`, `docs/build.md`, `docs/validation.md`, `docs/enhancements.md`, and relevant documents **when present**. Read existing files; do not assume a documentation pack accurately describes code already in the repository.
4. The active milestone's acceptance criteria and dependencies; identify the next smallest unfinished item.
5. The actual related source, tests, dependencies, and Windows build configuration.

Document responsibilities:

| Location | Responsibility |
| --- | --- |
| `AGENTS.md` | Durable mandatory engineering policy and stop conditions. |
| `docs/stack.md` | **Locked** libraries/backends and change-control policy; no spontaneous substitutions. |
| `docs/workflow.md` | **Binding** roadmap → milestone → task → subtask → acceptance definitions and update rules. |
| `docs/roadmap.md` | Approved milestone goals, dependencies and exit gates; change rarely and only with authorization. |
| `TODO.md` | **Only actionable work queue**: stable task/subtask IDs, statuses, dependencies, verification references. |
| `docs/architecture.md` | Target native boundaries and confirmed/unknown architectural decisions. |
| `docs/build.md` | Windows toolchain, local Debug/Release build contract, and Docker boundaries. |
| `docs/validation.md` | Reproducible local checks and parity acceptance protocol, not a claim of execution. |
| `VANILLABTN.md` | Native project entry point; the upstream `README.md` stays intact. |
| `docs/enhancements.md` | Future enhancements and completed enhancement work **after** bring-up; not a bring-up progress dump. |
| Code/tests/commits | The authoritative implementation and reproducible evidence. |

- **Preserve existing milestone IDs, history, lists, and writing style.** Never remove or renumber established milestones, including prior native milestones, merely to reorganize work. Integrate newly discovered work into the relevant existing milestone. If the documentation does not yet exist, create only the minimal document the current task genuinely needs.
- Follow the binding hierarchy in [`docs/workflow.md`](docs/workflow.md). `docs/roadmap.md` defines **what and in which order**; `TODO.md` defines **which R#-T## task and R#-T##-S## subtask is being executed and with what evidence**. Never turn roadmap rows into ad-hoc agent tasks or invent milestones. Keep statuses aligned with actual implementation, not intent.
- Keep the roadmap forward-looking; avoid duplicate audit documents, per-commit diary files, `native-rX.Y-*.md` reports, or lists of internal agent candidates.
- Do **not** insert the agent's environment, home directory, Linux/Windows test counts, skipped test narratives, or machine-specific validation claims into project documentation. Record durable technical facts and criteria; communicate ephemeral local test results in the final work report to the developer.
- Do not label something "verified on Windows" if the actual Windows host validation was not performed. Developer-owned Windows gameplay acceptance must be identified separately from agent-side checks.
- When a task reveals another actionable defect, place it in its appropriate existing milestone rather than silently expanding the current one.

## 4. Mandatory execution workflow

**Before any work:** the agent must identify **one** authorized `R#-T##` item, its corresponding `R#` roadmap gate, its dependencies and acceptance checks. The hierarchy and task definitions in [`docs/workflow.md`](docs/workflow.md) are mandatory. **Technology decisions in [`docs/stack.md`](docs/stack.md) are locked.** If a suitable task does not exist, or a locked technology cannot meet its requirement, request a narrowly scoped decision; do not silently expand the plan or substitute a dependency.


For each task or milestone:

1. **Read:** Re-check the documents, branch, changes, dependencies, and relevant code.
2. **Select:** Choose **one existing R#-T## task** from `TODO.md` under the active authorized roadmap milestone; honor dependencies and `docs/workflow.md`. Do not invent a competing work queue, milestones, or unrelated tasks.
3. **Scope:** State the expected outcome, what will remain unchanged, assumptions, and a small acceptance checklist. For larger work, divide it into independently verifiable atomic slices.
4. **Implement:** Make a minimal, reversible change; keep original PS1 references intact. Do not mix feature work, fixes, formatting, and unrelated docs in one commit.
5. **Verify locally:** Run only appropriate local commands, focused tests, format/lint checks, builds, comparisons, and manual checks supported by the available environment. Include negative/regression tests where behavior is changed. Do not claim unavailable checks were performed.
6. **Review:** Inspect `git diff --check`, complete diff, unexpected changed files, sensitive paths, generated artifacts, and the version increment. Verify every changed line is in scope.
7. **Document:** Update the **existing TODO task/subtasks** with factual state and durable acceptance references. Update the **roadmap only when the developer approves a goal/gate/dependency change**, never merely because a task advanced. Do not produce redundant reports.
8. **Commit atomically:** Use the Git and versioning rules below. A commit must leave the repository in a coherent, reviewable state; if a change cannot be verified, say so explicitly.
9. **Report:** Summarize what changed, files/commits, checks actually run, checks not run, risks, and exactly what remains. Do not claim the overall feature is finished until every milestone gate is met.
10. **Stop at approval gates:** At the end of a major feature, wait for the developer's approval and explicit PR/merge instructions. Do not self-approve, open a PR, or merge on the agent's initiative.

If an interruption occurs, start the next session by reconstructing state from Git and docs; never assume an earlier agent's summary is sufficient proof.

## 5. Git, branch policy, commits, and PRs

- **Use Conventional Commits** with concise, imperative subjects, e.g. `feat(renderer): implement PS1 texture palette upload`, `fix(assets): reject truncated archive index`, `test(runtime): cover deterministic tick ordering`, `docs(roadmap): update bring-up gate`.
- **Every authored commit must be atomic**: one logical objective, its directly related tests, necessary documentation, and the mandatory version update. Avoid "misc", "updates", or mega-commits.
- For a **major work package** spanning several milestones (for example the entire Windows bring-up across Rn through Rn+1 and beyond), create **one dedicated feature branch** from the agreed integration branch before changing files, e.g. `feat/windows-native-bringup`. Keep its related atomic commits together; do not create a new branch for every submilestone unless requested.
- Small scoped fixes should follow the existing branch policy. Never commit directly to a protected integration branch without the developer's authorization.
- Never commit, push, cherry-pick, amend, rebase, tag, revert, create a PR, or merge blindly. Inspect scope/history first. Never force-push or rewrite published history without explicit developer permission.
- **Major-work completion gate:** implement and verify each agreed acceptance criterion; show the developer the outcome and remaining limitations; **wait for explicit approval**. Make the final approved version/milestone commit only as directed. Then **wait for the developer to request a PR**; creating a PR is not automatic. **Merge only when and how the developer instructs**.
- A requested PR must explain scope, milestone evidence, known deviations/blockers, review steps, and compatibility implications; no unsupported success claims. Keep the existing upstream CI untouched.
- Prefer history-preserving integration instructions from the developer. If concurrent changes introduce version conflicts, reconcile them with the developer before integration; never resolve by silently overwriting the target branch's version.

## 6. Semantic versioning — STRICT

Version format: **`MAJOR.MINOR.PATCH`** using numeric, nonnegative components (no ad-hoc version strings).

- **MAJOR:** Set or changed **only by the developer**. An agent must not choose the initial MAJOR, increase MAJOR, or infer a major release from the scope.
- **PATCH:** **Increment by exactly 1 for every repository commit authored as part of this project**, including code, tests, documentation, and the final milestone commit. No commit may omit its patch bump. Do not treat Conventional Commit `feat`/`fix` types as automatic MINOR/MAJOR triggers.
- **MINOR:** Increment by exactly 1 **only** after the developer explicitly approves a completed major work package (such as Windows native bring-up). This increment belongs in the **last approved commit** on its feature branch, before the developer-requested PR and eventual merge.
- On that final commit, **increment both MINOR and PATCH**, because the project explicitly requires PATCH to advance on *every* commit. Example: `1.2.17` → ordinary commit `1.2.18` → developer-approved final milestone commit `1.3.19`. **Do not automatically reset PATCH to 0**: this project intentionally uses a monotonically increasing PATCH across minor releases. SemVer permits these numeric versions, although the policy differs from common release conventions.
- Keep **one existing authoritative version location**; update generated mirrors only as needed. If none exists or the initial version/MAJOR is unknown, **ask the developer to establish the baseline before authoring a commit**. Never invent a MAJOR or fabricate a version file without agreement.
- Check the previous committed version and Git state before every commit. When merging/rebasing concurrent branches, re-check monotonicity and resolve conflicts intentionally; do not hide missing or duplicate increments. GitHub-generated merge commits are integration events and are not a substitute for an authored versioned commit.
- No automatic tagging, changelog release publication, or release packaging without explicit developer instruction.

## 7. Windows-first toolchain and cross-OS correctness

**Primary target:** native Windows executable built with Rust on the developer's Windows machine.

- Use the stable Rust toolchain and Windows **MSVC target** (`x86_64-pc-windows-msvc`) by default unless the project records an approved alternative. The Windows host already has **Visual Studio Desktop development with C++** tools and **Clang** available; use their supported native toolchains when appropriate. Verify actual versions/availability rather than inventing them.
- Favor `cargo`, `rustfmt`, `clippy`, `rustc`, and existing project build tooling. **The technology stack in [`docs/stack.md`](docs/stack.md) is binding**: `winit` for window/input, `wgpu` multi-backend GPU (`dx12`, `vulkan`, `gles`/OpenGL, `metal` via `wgpu`, with D3D12 default on Windows), WGSL shaders, `kira`/`cpal` audio, `gilrs` gamepads, `glam` graphics math, stdlib asset/save I/O, and Debug-only `tracing` diagnostics. This policy selects the libraries; it does **not** require adding unused dependencies early. No agent may substitute a framework or non-`wgpu` renderer without developer approval.
- **Code formatting — MANDATORY:** Always keep all edited source, config, and documentation neatly formatted and aligned with `.editorconfig` and `rustfmt`. Before every commit touching Rust code, format with `cargo fmt` and verify `cargo fmt --check` passes cleanly with zero diff, alongside `git diff --check` for trailing whitespace or conflict markers. Keep untouched upstream C/ASM/build files unformatted.
- All new runtime code should be Rust by default. C/assembly or FFI may be retained as **explicitly approved temporary compatibility steps**, not as hidden permanent PS1 emulation. Do not make the Rust application depend on PS1 BIOS, MIPS interpretation, or original PSY-Q at runtime.
- **Docker:** Installed on the host, but **not part of the default Windows native Rust build or runtime**. Keep the upstream Dockerfile intact. Use Docker only when the developer requests or an existing task genuinely needs an isolated **local PS1 reference build/hash check**; do not introduce Docker requirements or containers for the native app. Do not remove upstream Docker support.
- **Existing CI:** Keep `.github/workflows/` and all current hmbtn-decomp CI definitions unchanged. **Never add, remove, edit, configure, or manually run CI**, including GitHub Actions, `act`, workflow dispatch, or new cloud build pipelines. Any existing upstream CI triggered automatically by the hosting service is outside this agent's workflow; all agent-initiated validation is local.
- Use OS-independent paths (`std::path::PathBuf`, `std::env`, `temp_dir`) and avoid shell-specific assumptions. Prefer portable build tools or explicitly separated Windows-local scripts; do not require WSL/Linux for the Windows product.
- Be aware of Windows path separators, case-insensitive filenames, reserved names, executable locking, long paths, Unicode, and differing filesystem semantics. Do not create case-only filename collisions.
- **Line endings:** Prefer **LF** in new source, Rust files, TOML, Markdown, YAML, JSON, scripts where supported, and Git-managed text files; use **CRLF** only where an existing Windows-specific format/script requires it (e.g. `.bat`/`.cmd` when applicable). Preserve the existing line endings of untouched upstream/decompiled files; never introduce whole-file diffs just by normalizing CRLF/LF.
- If needed, use a minimal `.gitattributes` policy in a separately scoped change after inspecting the existing repository. Never blanket-renormalize the project. Verify with `git diff --check` and inspect `git diff --ignore-space-at-eol` when diagnosing line-ending noise.
- Keep host-specific settings, tool output directories, caches, IDE metadata, game data, and binaries out of version control.

## 8. Build profiles — Debug versus Release (MANDATORY)

**Two intentionally different native Windows build profiles are required.** Follow Rust/Cargo's standard development and production practices. Profile selection must be compile-time, not merely a runtime `--quiet` switch. Never let diagnostics alter the game's normal logic or simulation results.

| Contract | Debug (`cargo build`) | Release (`cargo build --release`) |
| --- | --- | --- |
| Purpose | Development, diagnosis, local parity investigation | Shipping-quality native gameplay |
| Optimization | Unoptimized (`opt-level = 0`) | Optimized (`opt-level = 3`, unless measurements justify another level) |
| Debug symbols | Enabled for debugger/backtraces | Disabled in the distributable build; optional private symbols only if expressly approved |
| Debug assertions | Enabled | Disabled |
| Instrumentation | Debug-only checks, tracing, timing, and diagnostic helpers allowed | Debug-only implementation **not compiled into the executable** |
| Logging | Enabled; writes **`VanillaBTN.log`** | **No logging of any level**, no log file creation, no trace/profiling/debug console output |
| User-visible errors | Correct error handling plus useful diagnostic context in log | Correct error handling through UI/return status, never silently ignored |

- Use standard Cargo profiles (`[profile.dev]` and `[profile.release]`) and prefer their defaults unless explicit settings are needed to enforce this contract. Keep the configuration minimal; do not add custom build scripts or extra profiles without a demonstrated need. In Debug, keep optimizations off and symbols/assertions on. In Release, enable production optimizations and disable debug assertions and distributable debug symbols. Evaluate LTO, codegen units, symbol stripping, and `panic` behavior only when justified by actual constraints or measurements; do not cargo-cult tuning flags.
- **Compile out debugging in Release.** Gate debugging modules, diagnostic helpers, tracing spans, logging calls/macros, debug overlays, counters, profilers, and development-only checks using Rust compile-time conditions such as `#[cfg(debug_assertions)]` / `#[cfg(not(debug_assertions))]`, backed by explicit profile settings. Do not merely change log verbosity to `off`: the Release executable must not initialize a logger, open `VanillaBTN.log`, or retain a dormant debug subsystem. Avoid dependency features that cause logging/telemetry to remain active in Release; inspect any unavoidable third-party behavior.
- **Debug log contract:** Create or append to a file whose exact basename is `VanillaBTN.log`, under a writable per-user Windows location (recommended: `%LOCALAPPDATA%\VanillaBTN\VanillaBTN.log`), not a hardcoded machine-specific path, `Program Files`, or the Git working tree. Determine the directory via OS-appropriate APIs; create it if necessary and handle failures gracefully without crashing the game. Log useful, timestamped, leveled diagnostic events (startup/version/profile, module/asset errors, renderer/audio/save failures, and relevant game-state transitions) while avoiding noisy per-frame output by default. Ensure bounded file growth or documented local cleanup rather than unlimited logging. Flush/close on ordinary shutdown; best-effort flush on fatal errors without promising recovery from an abort or process termination.
- Never log secrets, credentials, user-identifying paths, full environment dumps, copyrighted asset bytes, or personal save content. Any Debug console diagnostics must be deliberate; prefer the file for routine diagnostics. Add `VanillaBTN.log` and related diagnostic artifacts to `.gitignore` when relevant, without touching unrelated ignore rules.
- **Release contract:** No application logging, diagnostic file output, debug HUD, debug console, tracing export, profiling hooks, or diagnostic-only work at runtime. Preserve normal gameplay-visible error handling; “no logging” does **not** mean swallowing failures. Third-party libraries must not create application diagnostic output or files in production. The platform may still surface OS-level crash reporting; do not claim to disable OS facilities outside the game's control.
- Do not confuse debug assertions with correctness. Bounds checks, input validation, archive integrity, safe save writes, and behavior essential to game fidelity/security must remain active in **both** profiles. Explicitly account for any arithmetic/overflow differences between unoptimized and optimized builds; require equivalent observable gameplay in Debug and Release. Never use `cfg!(debug_assertions)` alone to wrap heavyweight debug work when it can be eliminated completely with `#[cfg(...)]`.
- **Local acceptance check for each affected milestone:** Build and launch **both** variants with the Windows MSVC toolchain. Verify that Debug produces `VanillaBTN.log` with meaningful diagnostics and that Release runs correctly, creates/opens no `VanillaBTN.log`, and has no application-level debug/logging output. Test representative startup, asset loading, rendering, and gameplay states in both. If Windows execution was not possible, mark these checks **not verified**, not passed. Do not add or run CI to perform this validation.

## 9. Game fidelity and native architecture constraints

- Treat the matching PS1 decompilation as the reference for intended logic, constants, event order, data layouts, and asset interpretation. **Do not assume decompiled C can be compiled unchanged as native x64 code.** PS1 addresses, overlays, SDK calls, GPU primitives, SPU/XA audio, memory cards, and CD/file access need explicit native equivalents.
- Preserve *observable* original gameplay and presentation: simulation rate, ordering, input latency expectations, movement/collision, doors/scene transitions, rendering/blending, animation timing, UI, dialogue, RNG usage, clock/weather, NPC/animal routines, minigames, music/SFX, saving/loading.
- Keep deterministic simulation logically independent from presentation frame rate. Do not introduce uncapped simulation or change the game's speed just because the host GPU can render faster.
- Replace PS1-specific subsystems incrementally with small, testable native interfaces **using the approved stack** (`docs/stack.md`). Prefer transparent code and known behavior to speculative emulation layers or wholesale rewrites without intermediate validation.
- Respect resource ownership and platform APIs. Avoid unsafe Rust except at a proven hardware/FFI boundary; document the invariant and keep unsafe scopes minimal. Treat save writes and asset parsing as data integrity/security boundaries.
- Do not ship copyrighted disc images, extracted game assets, BIOS, proprietary SDK libraries, or user save files in Git, tests, release bundles, or PR attachments. Keep tests and fixtures lawful, small, and reproducible.
- Preserve working game paths when replacing them. Never delete a compatibility implementation until an equivalent native path is verified and rollback is possible.
- Avoid gameplay changes disguised as fixes; if parity is unclear, document the discrepancy and obtain a decision before intentionally changing behavior.

## 10. Bring-up roadmap — Windows AS-IS (no enhancements)

Use the following as the **goal/dependency sequence**, not permission to overwrite an existing roadmap. If the repo already has numbered milestones (including earlier M-series or R-series work), **map these goals onto those milestones and preserve their identifiers and history**. Only create new numbered items when no equivalent milestone exists. One dedicated feature branch may cover several consecutive milestones.

| Gate | Deliverable | Minimum acceptance criteria |
| --- | --- | --- |
| **R0 — Baseline** | Repository inventory, decomp/reference boundaries, source/version/branch rules, Windows build prerequisites, asset acquisition strategy. | Current code/docs and original reference checks are understood; baseline is documented without altering original bytes; unknowns are visible. |
| **R1 — Rust Windows boot** | Smallest native Rust workspace/executable, Windows window/event loop, input skeleton, safe shutdown, local build path. | Debug and Release Windows binaries build and launch locally; Debug creates `VanillaBTN.log`, Release compiles out logging and creates no log; no PS1 runtime or emulator needed. |
| **R2 — Assets** | Read required original disc/archive metadata and resources through native, bounded parsers. | Known assets are loaded and validated against reference data; missing/corrupt data fails clearly; copyrighted data remains external. |
| **R3 — Rendering** | Host GPU path matching PS1 primitives, palettes/CLUT, ordering, transparency, camera and viewport semantics. | Representative reference scenes/frames match accepted baseline behavior within documented tolerances; no visual enhancement yet. |
| **R4 — Core runtime** | Native lifecycle, deterministic simulation ticks, game state, relevant overlay/system transitions, title/menu flow. | Original startup and title-to-game transition execute natively in correct event order. |
| **R5 — World and player** | Maps, input actions, movement, collision, interaction, doors, camera, scene transitions. | Targeted gameplay routes match original control/state/transition behavior without regressions. |
| **R6 — Gameplay systems** | Farming, inventory, day/time/weather, NPCs, animals, schedules, events, commerce, minigames as applicable. | Each migrated system passes focused local checks and developer-observed original-game scenarios; known differences are tracked. |
| **R7 — Audio and persistence** | Native music/SFX, audio sequencing, save/load and any required memory-card compatibility. | Reference audio behaviors and save/load round trips work; no silent save format change or data loss. |
| **R8 — End-to-end parity** | Integration, stability, Windows Debug/Release validation, representative playthrough/regression coverage. | All agreed as-is scenarios and prior working flows pass on the developer's Windows host in both profiles; the Debug log and Release no-logging contract are validated; all blockers disclosed; developer explicitly accepts the bring-up. |

**R9+ enhancements are OUT OF SCOPE** until R8 is accepted and the developer authorizes a separately planned feature. No widescreen, frame-rate changes, texture upgrades, altered doors, game design fixes, quality-of-life additions, modding framework, or unrelated optimization in R0–R8.

Every milestone must have concrete `R#-T##` tasks, optional `R#-T##-S##` subtasks, prerequisites, reproducible local checks, measurable acceptance criteria, known limitations, and a status reflecting **evidence rather than enthusiasm**. `docs/workflow.md` is mandatory. Do not close a gate simply because later code was started.

## 11. Local verification and reporting

Allowed, when appropriate and available: focused `cargo test`, `cargo fmt --check`, `cargo clippy`, Windows Debug/Release builds and profile-specific logging checks, scoped game-data parsers/tests, native application runs, in-game observation, and explicitly requested **local** PS1 reference comparisons. These are **local checks**, not CI. Do not add a CI job to execute them.

Before claiming completion, verify the relevant minimum:

- Both Debug and Release compile and run on the intended Windows toolchain; Debug generates `VanillaBTN.log` and Release does not compile/include or emit application debug logging (or explicitly state that the developer must run this validation).
- Relevant unit/integration tests and regression scenarios pass; failure cases are tested for new parsers or save paths.
- Input, timing, graphics, audio, scene transitions, and persisted data are compared with the original where affected.
- No unintended changed files, unrelated refactors, copyrighted binary data, sensitive paths, or line-ending-only diffs are staged.
- TODO/roadmap reflects observed outcomes; the version matches the actual commit history; the feature branch remains reviewable.

In the final handoff message, distinguish **implemented**, **locally verified**, **requires developer Windows verification**, and **blocked/unverified**. Give exact reproducible commands only when those commands really apply. Do not claim execution, file inspection, or parity checks that did not occur. Never add agent-environment test logs to committed documentation.

## 12. Sensitive information and repository hygiene

- **Never commit or publish personal machine identifiers or absolute home paths**, including developer-specific home directories, user account names, temporary workspace paths, private domains, tokens, emails, credentials, license keys, or local device IDs.
- In docs, commands, sample configs, errors, and commit descriptions use portable forms such as `$HOME`, `~/`, `%USERPROFILE%`, `$env:USERPROFILE`, `<PROJECT_ROOT>`, `<GAME_DATA_DIR>`, and `<TEMP_DIR>` as appropriate. Do not blindly use a Unix variable inside Windows PowerShell commands.
- Review diffs, logs, screenshots, asset dumps, generated test snapshots, patch files, and PR descriptions for accidental secrets or environment-specific data.
- Keep `.env`, credentials, original disc contents, extracted proprietary assets, local saves, build outputs, IDE caches, and temporary reports outside tracked Git content; inspect `.gitignore` before adding files.
- Never weaken security checks, silently bypass validations, or use generated data as ground truth just to make tests pass.

## 13. Final pre-commit / pre-PR checklist

- [ ] Read `AGENTS.md`, `docs/stack.md`, `docs/workflow.md`, roadmap, TODO, branch state, and changed source; identify **one existing task ID**.
- [ ] Work corresponds to an agreed **R# milestone and R#-T## task** (plus linked subtasks); no invented work, no stack substitution, no enhancements.
- [ ] Original PS1 source/reference bytes and upstream CI remain untouched.
- [ ] Change is simple, minimal, atomic, cleanly formatted (`cargo fmt --check` & `git diff --check`), and Windows-aware; line endings reviewed.
- [ ] Debug/Release build separation checked locally where possible: Debug `VanillaBTN.log` exists; Release has no application logging/debug instrumentation; unsupported checks honestly identified.
- [ ] No secrets, personal paths, copyrighted assets, or generated junk in the diff.
- [ ] TODO task/subtask statuses/evidence updated; roadmap unchanged unless the developer explicitly approved a gate change; no extra audit files.
- [ ] Conventional Commit message is accurate; **PATCH advances by 1**.
- [ ] No MINOR bump without developer approval; no MAJOR decision by the agent.
- [ ] No PR until requested; no merge until explicitly instructed.

**Default decision rule:** If an action is not needed for the current as-is Windows bring-up milestone, do not do it. If evidence is missing, say what is missing instead of pretending certainty.
