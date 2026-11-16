# Windows Native Bring-up Roadmap (As-Is)

**Project:** VanillaBTN · **Current scope:** R0–R8 · **State of this plan:** proposed gates; none are claimed verified by this documentation package.

## Outcome and boundary

Deliver a usable Rust-native Windows build that reproduces the original game's observable startup, visual, input, timing, gameplay, audio and save/load behavior without running a PS1 emulator/BIOS/MIPS interpreter. Source C, CMake, Docker and upstream CI remain reference material. Modernization and gameplay enhancements are separate future work.

**Progress authority:** [`../TODO.md`](../TODO.md). **Engineering rules:** [`../AGENTS.md`](../AGENTS.md), locked [`stack.md`](stack.md), binding [`workflow.md`](workflow.md). This roadmap is the **approved-order plan and milestone acceptance contract**, NOT a task backlog. Only TODO carries tasks/subtasks and their status/evidence. Do not create competing per-agent milestone lists or renumber pre-existing milestones when importing this roadmap into an active project.

## Dependency gates

| Gate | Deliverable | Required evidence before `DONE` |
| --- | --- | --- |
| **R0 — Baseline** | Source/reference inventory, version/branch decision, Windows toolchain inventory | Exact source boundaries checked, agreed baseline, environment prerequisites established by local inspection. |
| **R1 — Native bootstrap** | Rust executable, window/input/shutdown, Debug and Release | Both binaries **built and launched on Windows MSVC**; Debug log works, Release includes no application debug logging. |
| **R2 — Assets** | Native reads/parsers for original resources | Valid reference data reads correctly, bad data safely rejected, proprietary resources not tracked. |
| **R3 — Rendering** | Host GPU pipeline reproducing original primitives and presentation | Selected source frames/scenes compared with the original and deviations recorded. |
| **R4 — Runtime** | Native lifecycle, fixed simulation tick and game-state flow | Title/startup transitions and event sequencing behave as reference. |
| **R5 — World/player** | Maps, character, collision, interaction, camera and doors | Representative movement routes and transitions validated as-is. |
| **R6 — Gameplay** | Farming, clock/weather, NPC/animal, events and other game systems | Feature-specific reference scenarios/checks pass; remaining gaps tracked. |
| **R7 — Audio/save** | Original sound behavior, safe game persistence | Audio scenarios and save/load round trips checked; no silent data-loss or format changes. |
| **R8 — Parity/acceptance** | Integrated Windows Debug+Release as-is experience | Developer-host manual acceptance, local checks, unresolved differences disclosed, **developer explicitly approves**. |

A later milestone is **not automatically authorized** merely because a developer/agent notices a possible shortcut. Respect prerequisite gates and the current authorized work package; request an explicit exception before advancing across a blocked dependency. Integration tests do not replace original behavior verification.

## Roadmap change control

- **Milestone** = R0–R8 outcome and exit criteria, not a commit, task, subtask, or an agent-invented "candidate". Preserve IDs and descriptions across updates.
- Roadmap edits require developer permission when they change gate order, scope, dependencies, architecture commitments, acceptance requirements or introduce new milestones. Task progress alone belongs in `TODO.md`.
- Agent discovery of missing work: add a narrowly scoped `R#-T##` task under an **existing milestone** only when it is genuinely required by approved criteria; if it alters requirements/priority/architecture, **propose and wait**.
- Milestone exit is evidence-based and different from developer acceptance of the overall R0–R8 bring-up. See [`workflow.md`](workflow.md).

## Explicitly out of scope before acceptance

No widescreen, uncapped simulation, graphical upgrades, new animations/door behavior, HD texture replacement, redesigns, gameplay balancing, new systems or modding frameworks. Do not mix deferred enhancements with bring-up fixes or hide intentional behavior changes behind a compatibility label.

## Completion and Git gates

1. Each actionable step is tracked in `TODO.md` and mapped to the existing project milestone IDs if any.
2. At each completed slice, run relevant **local** checks and record only durable, reproducible evidence. Do not change or run upstream CI.
3. Keep atomic Conventional Commits, with **PATCH +1 on each authored commit**. The developer chooses MAJOR; if initial version is unknown, ask rather than invent it.
4. After the developer approves the **whole major work package**, make the last approved commit with **MINOR +1 and PATCH +1**.
5. **Stop.** Open a PR only when requested, then merge only as explicitly instructed.

## Decision points (do not guess)

- Baseline and location of authoritative SemVer source.
- Accepted native runtime location / source-tree placement. **Renderer and audio choices are already locked in [`stack.md`](stack.md), not open to agent discretion.**
- Exact original-release/asset and save-compatibility requirements.
- Agreed parity tolerances for GPU output and game timing.
- Work-package acceptance, PR and merge permission.
