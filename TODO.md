# VanillaBTN — TODO (Actionable Tasks Only)

> Proposed **starter task breakdown**, not a claim that these tasks are missing/untested in the actual checkout. **Reconcile with existing repository TODO/IDs/history rather than overwriting existing work.** This is the ONLY agent work queue; [`docs/roadmap.md`](docs/roadmap.md) is the non-operational milestone plan. Read [`docs/workflow.md`](docs/workflow.md) for definitions and state transitions, [`docs/stack.md`](docs/stack.md) for locked stack.

## Active work and status rules

**Current phase:** R0–R8 Windows Rust native **as-is**; **no enhancements**. **Active milestone:** R0 (proposed; verify repository state first). **Active task:** none selected by this pack. No Windows build, test or game parity check is claimed. All checkboxes are initially unchecked intentionally.

- **Use only task IDs:** `R#-T##` (task), `R#-T##-S##` (subtask). Checkboxes correspond to completion evidence, never to guesses or enthusiasm.
- Task status: `TODO`, `IN PROGRESS`, `BLOCKED`, `DONE`, `CANCELLED (developer only)`. One active task per agent unless authorized. Every task must retain **Depends on**, **Acceptance**, and **Evidence**. Use `Evidence: —` until actually verified.
- For a task, update only its subsection/checkboxes and short evidence references. Do **not** regenerate milestone numbering, rewrite roadmap gates, erase completed items, promote subtasks to unrelated tasks, create parallel candidate queues, or append per-agent diaries.
- If actual checkout already contains tasks or milestone numbering, **preserve those exact IDs/history**; adapt these proposed items to that plan only after inspection. The developer must authorize substantive changes of gates, priorities, stack or scope.

## R0 — Inventory and reference baseline

**Milestone:** PLANNED · **Exit:** reference and environment boundaries checked; baseline/branch/version decisions explicit.

- [ ] **R0-T01 — Inspect source, Git and existing plans**  
  Status: TODO · Depends on: none  
  Scope: Read-only source/Git/docs inventory; do not modify upstream.  
  Acceptance: upstream build tree/CI/reference boundaries, prior TODO/roadmap and current Git state inventoried without modifications  
  Evidence: —
  - [ ] R0-T01-S01 — Inspect Git branch/status/version source and existing documentation
  - [ ] R0-T01-S02 — Inspect upstream C/PSY-Q build, Docker, workflows, asset/reference boundaries
- [ ] **R0-T02 — Confirm Windows toolchain and build strategy**  
  Status: TODO · Depends on: R0-T01  
  Scope: Windows Rust/MSVC, VS C++ SDK and optional reference-tool inspection only.  
  Acceptance: local Rust MSVC / Windows SDK / VS C++ toolchain checked; native build excludes Docker; optional original PS1 reference path documented  
  Evidence: —
- [ ] **R0-T03 — Agree native placement, reference and version baseline**  
  Status: TODO · Depends on: R0-T01  
  Scope: Decision on native directory, source data, Git branch and version baseline only.  
  Acceptance: developer has decided authoritative MAJOR/version source (if not already set), feature branch and native code location; legally sourced reference inputs identified  
  Evidence: —

## R1 — Minimal Rust Windows executable

**Milestone:** PLANNED · Depends on: R0 · **Exit:** native window/input/shutdown; Debug and Release behavior confirmed on developer Windows.

- [ ] **R1-T01 — Minimal Rust workspace + winit native boot**  
  Status: TODO · Depends on: R0-T02, R0-T03  
  Scope: Small executable, window loop and shutdown only; no gameplay/renderer yet.  
  Acceptance: smallest Rust/MSVC app opens/closes window, processes native events; no PS1 runtime dependency  
  Evidence: —
- [ ] **R1-T02 — Separate Cargo build profiles and Debug diagnostics**  
  Status: TODO · Depends on: R1-T01  
  Scope: Cargo dev/release profiles and compile-time gated file diagnostics only.  
  Acceptance: Debug unoptimized with bounded `VanillaBTN.log` via compiled-in diagnostics; Release optimized and diagnostics compiled out; operational error handling preserved  
  Evidence: —
  - [ ] R1-T02-S01 — Profile settings and compile-time conditional call sites
  - [ ] R1-T02-S02 — Debug-only per-user log location and rotation/bounded growth
- [ ] **R1-T03 — Local Windows boot verification**  
  Status: TODO · Depends on: R1-T01, R1-T02  
  Scope: Actual two-profile Windows startup and logging verification only.  
  Acceptance: Windows MSVC Debug and Release binaries actually launched; Debug log and Release no-log confirmed; report unverified until dev-host evidence exists  
  Evidence: —

## R2 — Native asset acquisition and parsing

**Milestone:** PLANNED · Depends on: R1 · **Exit:** legally supplied original resources parsed with checked bounds.

- [ ] **R2-T01 — Trace original disc/archive index and startup resources**  
  Status: TODO · Depends on: R0-T01, R1-T01  
  Scope: Source-based layout and needed startup asset mapping only.  
  Acceptance: original asset layouts/types mapped from source; unknown formats recorded, not guessed  
  Evidence: —
- [ ] **R2-T02 — Implement bounded Rust read-only parser for required startup assets**  
  Status: TODO · Depends on: R2-T01  
  Scope: Read-only parser limited to required startup asset types and negative tests.  
  Acceptance: known reference entries parse consistently; corrupt/truncated inputs reject safely; no copyrighted data committed  
  Evidence: —

## R3 — GPU rendering baseline

**Milestone:** PLANNED · Depends on: R2 · **Exit:** representative original frames drawn natively with documented parity.

- [ ] **R3-T01 — Bootstrap multi-backend wgpu and WGSL**
  Status: TODO · Depends on: R1-T01, R2-T02  
  Scope: Initial `wgpu` surface/pipeline across supported native backends (DX12 default on Windows, Vulkan, OpenGL/GLES, Metal) and a minimal verified draw path.
  Acceptance: native surface/device/pipeline presents required startup geometry across available host hardware backends; no non-`wgpu` renderer
  Evidence: —
- [ ] **R3-T02 — Original textures, palettes and CLUT rules**  
  Status: TODO · Depends on: R3-T01, R2-T02  
  Scope: PS1 texture palette/CLUT lookup without post-processing.  
  Acceptance: texture sampling/color translation compared with reference and edge cases tested  
  Evidence: —
- [ ] **R3-T03 — Original drawing order, blending, geometry and viewport**  
  Status: TODO · Depends on: R3-T02  
  Scope: Original geometry/order/blend/viewport behavior for selected test frames.  
  Acceptance: reference frame comparisons cover draw ordering/sem transparency and viewport semantics; deviations explicit  
  Evidence: —

## R4 — Native game loop and title flow

**Milestone:** PLANNED · Depends on: R3 · **Exit:** verified startup/title → game entry state sequence.

- [ ] **R4-T01 — Original initialization and deterministic simulation scheduling**  
  Status: TODO · Depends on: R3-T03  
  Scope: Game initialization and evidence-based fixed simulation sequence.  
  Acceptance: event/update order and tick semantics supported by evidence; render frame rate cannot alter gameplay time  
  Evidence: —
- [ ] **R4-T02 — Original title/menu and state transitions**  
  Status: TODO · Depends on: R4-T01  
  Scope: Startup/title/menu state path; no new UI behavior.  
  Acceptance: observed native startup/title/menu transitions align with original flow  
  Evidence: —

## R5 — World, player and interaction

**Milestone:** PLANNED · Depends on: R4 · **Exit:** original player movement, camera and transitions.

- [ ] **R5-T01 — Player input, map and collision semantics**  
  Status: TODO · Depends on: R4-T02  
  Scope: Player action mapping, required world data and collision.  
  Acceptance: movement/collision/controller action mapping matches reference scenarios  
  Evidence: —
- [ ] **R5-T02 — Interactions, doors, camera and scene transitions**  
  Status: TODO · Depends on: R5-T01  
  Scope: Original object use, camera and unchanged door/scene transition logic.  
  Acceptance: representative routes and door transitions match **original** game (no animation enhancement)  
  Evidence: —

## R6 — Core gameplay systems

**Milestone:** PLANNED · Depends on: R5 · **Exit:** required original gameplay systems each have scoped parity evidence. **Do not treat the entire gameplay port as one task.**

- [ ] **R6-T01 — Inventory/items operations**  
  Status: TODO · Depends on: R5-T02  
  Scope: Original item/inventory representation and interactions.  
  Acceptance: item states, inventory actions and limits match source/reference cases  
  Evidence: —
- [ ] **R6-T02 — Farming/crop and tool operations**  
  Status: TODO · Depends on: R6-T01  
  Scope: Farming/crop/tool operations without balancing or mechanics changes.  
  Acceptance: farm action/crop/tool transitions match original scenarios  
  Evidence: —
- [ ] **R6-T03 — Game clock, day and weather progression**  
  Status: TODO · Depends on: R4-T01  
  Scope: Original day/clock/weather update rules only.  
  Acceptance: original update order and representative time/weather transitions verified  
  Evidence: —
- [ ] **R6-T04 — NPC movement, interaction and schedules**  
  Status: TODO · Depends on: R5-T02, R6-T03  
  Scope: Original NPC routines/interaction transitions only.  
  Acceptance: selected NPC schedule and interaction scenarios match original  
  Evidence: —
- [ ] **R6-T05 — Animal lifecycle and interactions**  
  Status: TODO · Depends on: R6-T01, R6-T03  
  Scope: Animal state transitions and related interactions only.  
  Acceptance: representative animal state transitions match original  
  Evidence: —
- [ ] **R6-T06 — Event triggers, sequencing and dialogue**  
  Status: TODO · Depends on: R6-T03, R6-T04  
  Scope: Event triggers, order, outcomes and original dialogue handling.  
  Acceptance: selected original event trigger/order/outcome sequences verified  
  Evidence: —
- [ ] **R6-T07 — Shops, economy and item transactions**  
  Status: TODO · Depends on: R6-T01  
  Scope: Original in-game economy, shopping and inventory transfer rules.  
  Acceptance: price, stock, balance and transaction state cases match reference  
  Evidence: —
- [ ] **R6-T08 — Remaining original in-game menus**  
  Status: TODO · Depends on: R4-T02, R6-T01  
  Scope: Existing in-game menus only; no new layouts or usability changes.  
  Acceptance: original menu/input/state transitions covered for identified menus  
  Evidence: —
- [ ] **R6-T09 — Remaining original minigames**  
  Status: TODO · Depends on: R6-T08, R5-T02  
  Scope: Required existing minigames only, as inventoried from reference.  
  Acceptance: identified minigames and their win/loss/state transitions have reference scenarios; gaps stated, not hidden  
  Evidence: —

**Inventory is mandatory:** R6-T09 may close only after the developer-agreed original feature coverage list is inspected and missing gameplay features are allocated to the appropriate **existing** R6 tasks or explicitly approved new R6 tasks. The agent may not shorten the coverage list to claim R6 complete.

## R7 — Native audio and persistence

**Milestone:** PLANNED · Depends on: required R2–R6 resources/state · **Exit:** original cues, timing and safe compatible saving verified.

- [ ] **R7-T01 — Inspect original audio assets and sequence behavior**  
  Status: TODO · Depends on: R2-T02, R4-T01  
  Scope: Original sound asset/format and cue mapping; no implementation yet.  
  Acceptance: necessary music/SFX formats and original event timing traced to evidence; unsupported formats explicitly identified  
  Evidence: —
- [ ] **R7-T02 — Implement required original audio format decoder(s)**  
  Status: TODO · Depends on: R7-T01  
  Scope: Only observed audio encodings needed for existing content.  
  Acceptance: observed formats decode correctly against reference samples; malformed/truncated data rejected  
  Evidence: —
- [ ] **R7-T03 — Host mixing and playback via kira/cpal**  
  Status: TODO · Depends on: R7-T02, R4-T01  
  Scope: kira/cpal output, original mixing and scheduling behavior only.  
  Acceptance: Windows output/cue/loop/sequence behavior compared with original; no alternate audio engine  
  Evidence: —
- [ ] **R7-T04 — Save/load compatibility and integrity**  
  Status: TODO · Depends on: R6-T01, R6-T03, R6-T06  
  Scope: Compatible original save/load and robust handling of invalid/failed writes.  
  Acceptance: reference save semantics preserved; roundtrip and corrupt/partial I/O cases tested on Windows  
  Evidence: —

## R8 — Windows as-is acceptance

**Milestone:** PLANNED · Depends on: R0–R7 · **Exit:** developer acceptance of integrated Windows-native original-behavior build.

- [ ] **R8-T01 — Integrated native parity scenarios and regression review**  
  Status: TODO · Depends on: R3-T03, R4-T02, R5-T02, R6-T01, R6-T02, R6-T03, R6-T04, R6-T05, R6-T06, R6-T07, R6-T08, R6-T09, R7-T03, R7-T04  
  Scope: Existing as-is scenarios across required subsystems; no enhancements.  
  Acceptance: agreed representative playthrough/input/render/audio/save scenarios and unknown differences recorded  
  Evidence: —
- [ ] **R8-T02 — Debug/Release and repository hygiene verification on Windows**  
  Status: TODO · Depends on: R8-T01, R1-T03  
  Scope: Windows profiles and repository/diff hygiene acceptance checks only.  
  Acceptance: both builds tested, debug log vs release no log verified; assets/PS1 sources/CI unchanged; local checks documented honestly  
  Evidence: —
- [ ] **R8-T03 — Developer approval, final version and PR stop gates**  
  Status: TODO · Depends on: R8-T01, R8-T02  
  Scope: Developer acceptance and final approved version/PR/merge gate only.  
  Acceptance: developer explicitly approves work package; final approved commit advances MINOR +1 and PATCH +1, then waits for **developer-requested PR** and later **developer-instructed merge**  
  Evidence: —

## Deferred — not agent work

No enhancements during R0–R8. The agent cannot create `R9`, a second TODO, or a speculative candidate section. Record truly deferred ideas in the pre-existing enhancements document only when appropriate/asked, with no implementation until developer authorization.
