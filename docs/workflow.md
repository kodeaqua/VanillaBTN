# Planning and Execution Workflow — VanillaBTN (Binding)

> All agents must follow this hierarchy. A roadmap milestone is **not** a task, a task is **not** a subtask, and passing a test is **not** developer acceptance. See [`../AGENTS.md`](../AGENTS.md) for Git/SemVer/CI policy and [`stack.md`](stack.md) for locked technology choices.

## 1. Hierarchy, ownership and change authority

| Level | Location | Definition / owner | When to edit |
| --- | --- | --- | --- |
| **Vision / scope** | `VANILLABTN.md`, `AGENTS.md` | Developer-established product constraints and project-wide agent rules | Only when developer explicitly changes direction. |
| **Technology decisions** | `docs/stack.md` | **Binding** runtime/library/backend choices | Only with developer approval after a justified change request. |
| **Roadmap** | `docs/roadmap.md` | Ordered **milestones R0–R8**, high-level deliverables, prerequisites and exit gates; **not** a daily checklist | Rarely: approved changes to goals, order, dependencies or acceptance criteria. |
| **Milestone** | A named `R#` row in roadmap; corresponding section in `TODO.md` | A measurable, cross-task deliverable (e.g. R3 native renderer) | Gate cannot be closed until all its mandatory tasks/criteria pass. |
| **Task** | `TODO.md`, `R#-T##` | One atomic, reviewable work outcome under exactly one milestone; has dependencies, criteria and status | Track whenever implementing, discovering a blocker, or verifying evidence. |
| **Subtask** | `TODO.md`, `R#-T##-S##` | A small step *inside* one task; **never** an independent feature/milestone | Add only when needed for work clarity; inherits parent scope. |
| **Acceptance check** | Under task and `docs/validation.md` | Repeatable proof that observable behavior/requirement is met | Mark pass only with real evidence; checked source or commands actually run. |
| **Deferred idea** | Existing `docs/enhancements.md` if available | Explicitly **not authorized** in R0–R8 | Record only when useful; do not implement or turn into a new task during bring-up. |

**Single source of truth:** roadmap = **what/why/order/exit criteria**; TODO = **who is working on what right now, task/subtask state and evidence**; validation = **how to prove**; stack = **with what technology**. No parallel planning trackers, ad-hoc `native-rX.Y-*.md` reports, invented `R9` milestone, duplicating roadmap gates as a separate "candidate backlog", or redefining roadmap inside a task note.

## 2. Stable IDs and statuses

- Milestones are fixed **R0–R8** for current bring-up. Existing developer/project IDs take precedence if importing these docs; **merge, never mass-renumber or erase history**.
- Task IDs are `R3-T01`, `R3-T02`, etc. Subtask IDs are `R3-T02-S01`. Keep IDs stable across edits; never reuse an ID for a different outcome or reassign a task to hide its history.
- A task should be small enough for focused review and meaningful verification; **do not group an entire engine, subsystem family, or milestone into one task**. If it requires several independent behaviors, split into stable tasks inside the existing milestone; do not hide it as a stack of vague subtasks.
- Tasks have one of `TODO`, `IN PROGRESS`, `BLOCKED`, `DONE`, `CANCELLED (developer only)`. `CANCELLED` needs developer authorization and a rationale, and should remain visible; not an agent cleanup operation.
- A milestone is `PLANNED`, `ACTIVE`, `BLOCKED`, `READY FOR ACCEPTANCE`, or `ACCEPTED` (last state requires developer approval when its exit criterion demands it). Roadmap keeps gates, TODO keeps evidence. A task's `DONE` does **not** automatically mark its milestone `ACCEPTED`.
- Only **one active task at a time** per agent unless the developer explicitly assigns independent parallel work. A task may have multiple subtasks, but do not use them to expand scope.
- Ordering: highest-priority **unblocked task in the current milestone**, respecting actual prerequisites. Do not silently skip. If another task is blocking, show the dependency and stop or perform an already-authorized independent task; do not fabricate a workaround project.

## 3. Task contract (minimum required fields)

Each task in `TODO.md` must identify:

```text
- [ ] R3-T02 — Reproduce original CLUT palette sampling
  Status: TODO
  Depends on: R2-T02, R3-T01
  Scope: Decode/upload palette data for the agreed scene only
  Acceptance: Reference palette sample matches agreed expected pixels;
              malformed palette input rejected safely
  Evidence: — (fill only after actual checks)
  - [ ] R3-T02-S01 — Map the original palette indexing from decomp
  - [ ] R3-T02-S02 — Implement the smallest wgpu path
```

An unchecked checklist is **not** proof that its implementation is missing. Reconcile with real source history first. Avoid `DONE` based on a commit message alone; link an actual test/manual check and, when applicable, developer Windows observation. Status and checkboxes must agree; explain intentionally blocked subtasks.

## 4. Strict session loop

1. **Reconstruct facts:** read `AGENTS.md` → `docs/stack.md` → `docs/workflow.md` → roadmap → `TODO.md` → other relevant docs; inspect Git status/branch/recent commits and actual source. Do not trust a previous agent summary.
2. **Select existing task:** quote a single `R#-T##` and its parent R#, dependencies, expected deliverable and acceptance checks. If no suitable task exists, propose the smallest new task **under an existing authorized milestone**, and ask approval when it changes scope, architecture, priority, or gates. Do not create a new R#.
3. **Plan bounded slices:** name relevant subtasks and touched files; explicitly state out of scope, expected verification and whether Windows developer checks will be needed.
4. **Implement one task:** focused code, focused tests, no unrelated formatting/refactors, no stack substitutions or enhancements. If blocked, capture factual reason and seek developer decision; do not change course in secret.
5. **Check locally:** run the relevant local test/build/profile checks available; inspect diff including original decomp tree, CI, assets, secrets, line ending noise. Never claim a host check from an agent container.
6. **Update TODO only:** update the task/subtask boxes, status, dependencies and *durable evidence references*. Edit the roadmap **only if an authorized goal/gate changed**; progress alone is NOT a reason to rewrite roadmap. Avoid machine-specific reports in docs.
7. **Atomic commit:** include implementation, direct tests, necessary TODO updates and **PATCH +1** within one logical Conventional Commit; follow authorization and branch policy. If baseline MAJOR/version source is unknown, stop before committing and request decision.
8. **Handoff:** report task ID, changed behavior, exact checks executed and not executed, evidence/risks, next existing task. Never report the parent milestone done simply because this task ended.
9. **Milestone gate:** when all mandatory tasks and evidence are complete, mark `READY FOR ACCEPTANCE`; inspect global exit criteria and dependencies. Only progress to a dependent milestone where authorized and gates allow it; R8 acceptance of the major work package always requires explicit developer approval. Then follow version/PR/merge stop gates.

## 5. No scope drift — stop / ask / record

- **New bug in same task:** fix only if directly required to meet task acceptance and bounded; otherwise record under the appropriate existing milestone and report it.
- **New legitimate prerequisite:** add an explicitly linked subtask/task within current approved scope (with reason), not a new milestone; ask before reprioritizing the work package.
- **Missing source/reference or uncertain behavior:** mark `BLOCKED` or `UNVERIFIED` and state required evidence; never backfill guessed values or fake parity.
- **Requested feature not in R0–R8:** do not work on it during bring-up; record as a deferred idea only when the developer requests.
- **Stack conflict:** follow `docs/stack.md`'s change request procedure. A task cannot authorize a new stack by declaring it necessary.
- **Developer direction beats docs**, but if it contradicts current version/gates/stack, explicitly reconcile the docs and scope before implementing; never silently override.
- **No fake activity:** planning changes, creating modules, compiling, testing, and developer acceptance are different outcomes. Use accurate status terminology.

## 6. Concrete example: R3 renderer task

**Correct sequence:** R3-T01 (initialize multi-backend `wgpu` pipeline and verify active hardware backends) → R3-T02 (CLUT sampling) → R3-T03 (draw order/blend) → R3 acceptance frame comparison. Each task is one reviewable implementation, with tests and evidence, one or more atomic commits if necessary, all under R3. An unapproved switch to Bevy or a non-`wgpu` renderer is **not** an acceptable "subtask". Updating `docs/roadmap.md` on every commit is **not** required and should not happen.

**Do not close R3** merely because it displays a textured triangle. Do not start R4 as the agent's new priority while required R3 items remain blocked without an approved, explicitly recorded exception.
