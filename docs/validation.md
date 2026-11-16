# Local Verification and Parity Protocol

> **Checklist, not a test report.** A checked condition must be backed by a test run, source inspection, or explicit developer observation. Never enter fictitious numbers or agent-environment details in committed docs. Map each check to a stable `R#-T##` task in `TODO.md`, per [`workflow.md`](workflow.md).

## Evidence categories

- **Implemented** — code exists; not sufficient to claim verified functionality.
- **Locally verified** — exact local commands/checks actually executed with known outcome.
- **Developer Windows verified** — the developer has run/observed the native game on the intended Windows host.
- **Unverified / blocked** — check not run, relevant data unavailable, or behavior unresolved.

Use `TODO.md` for durable **task/subtask** status and concise evidence references; roadmap gate definitions must not be rewritten to claim progress. Report temporary command results to the developer in the work handoff; do not add per-commit diary documents.

## Native boot and build matrix

| Scenario | Debug must satisfy | Release must satisfy |
| --- | --- | --- |
| Compile | Windows/MSVC build succeeds, no optimization required | Windows/MSVC optimized build succeeds |
| Start/exit | Creates window and exits cleanly | Creates window and exits cleanly |
| Debug instrumentation | Compiled; useful timestamped log events | Logging/debug modules compiled out; no diagnostic output |
| `VanillaBTN.log` | Created/updated in writable user-data directory | Not created, opened, appended or otherwise written by app |
| Error handling | Meaningful user-facing behavior plus optional diagnostics | Meaningful user-facing behavior **without** logging |
| Gameplay state | Agreed original transitions | Same expected state/logic despite optimization |

An old Debug log may remain on disk; compare changes/handles rather than interpreting mere existence as Release activity. Do not erase local user files for testing. Manually compare representative startup and game scenarios **in both** profiles.

## Progression-specific original-behavior checks

| Gate | Minimum check to design/execute |
| --- | --- |
| R2 | Correct asset index/length and read boundaries; invalid/truncated/missing files reject cleanly. |
| R3 | Original screen area, palette/CLUT, draw order, transparency and known reference frames. |
| R4 | Initialization, menu/title transitions, fixed simulation tick, RNG and event ordering. |
| R5 | Player movement, collision, interaction, camera and original door/area transitions. |
| R6 | Farm operations, time/weather progression, NPC/animal schedules and events. |
| R7 | Music/SFX expected cues; save/load round trip, invalid save handling and compatibility. |
| R8 | Integrated representative playthrough, both profiles, asset hygiene, disclosed differences. |

Use reproducible inputs and a reference build or legitimate original game observation. Comparing a rewritten system to its own output alone cannot establish original-game parity. Preserve originals; modifications, widescreen and high-FPS enhancements are not success criteria.

## Stop conditions

Do **not** close a gate if:

- the Windows host behavior was not actually verified when required;
- differences from original are unexplained and unapproved;
- one build profile passes but the other fails a required check;
- proprietary assets, local logs, private paths or an unintended source/CI change enter the patch;
- there are failing or unrun **required** checks without an explicit documented blocker.

## Handoff protocol

For each task, tell the developer what changed, which atomic commit(s) correspond to it, which local commands/scenarios were **actually** run, outstanding failures/risks, and the next TODO. Distinguish implemented from verified. Never open PRs or merge until the developer expressly instructs it, and never run CI.
