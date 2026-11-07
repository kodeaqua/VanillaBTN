# Working in VanillaBTN

## Project and scope

This is a matching C decompilation of Harvest Moon: Back to Nature for
PlayStation, USA (`SLUS_011.15`). The default goal is to preserve the original
executable and all 13 overlays byte for byte. Read [README.md](README.md),
[docs/architecture.md](docs/architecture.md), and [docs/building.md](docs/building.md)
before changing unfamiliar systems.

## Code navigation

- `src/main/main.c`: hardware initialization, scene dispatch, and frame loop.
- `src/main/`: resident engine code and shared game state.
- `src/main/game/farm_state.h`: `FarmState`, `game_state`, and farm working buffers.
- `src/overlay/11_GF_FARM/`: main gameplay; start with `farm_main.c` and `farm_frame.c`.
- `src/overlay/`: title, minigames, save/load, debug, credits, and memory card code.
- `src/include/`: shared types, item IDs, actor types, and hardware helpers.
- `cmake/`, `tools/`, `sdk/`: build rules, conversion tools, and SDK object metadata.
- `disc/`: expected hashes, archive members, and disc layout.

## Change rules

- Follow neighboring C style and the existing GCC 2.95.2-compatible conventions.
  Do not introduce modern compiler assumptions or host-sized type assumptions.
- Preserve struct layouts, integer widths, signedness, data order, source link
  order, common/BSS handling, overlay regions, and `GAP` entries unless the task
  specifically requires changing them. Even equivalent C can change machine code.
- Headers generally live beside their source and use bare-name includes. If adding
  a directory or source file, inspect both root and local `CMakeLists.txt` rules.
- Do not change reference hashes to make a mismatch pass. Diagnose the difference.
- If gameplay modification is explicitly requested, explain that intentional binary
  changes will fail the original matching check; report that result accurately.
- Do not commit game data, disc images, downloaded Sony SDK files, or build outputs.
  Preserve the license and attribution of `tools/maspsx`.
- Keep unrelated user changes intact. Do not normalize the entire working tree's
  line endings as a workaround for a local build issue.

## Branches, pull requests, and review

- Work organized around a specific topic or phase, including its tasks and
  subtasks, must use a dedicated branch instead of `main`. Create the branch from
  the latest `main` before starting changes. Use a descriptive name such as
  `feat/farm-events` or `docs/phase-1-onboarding`.
- Keep changes for that topic or phase on its branch. Use separate branches and
  PRs for independent topics or phases; dependent subtasks may share the parent
  topic or phase branch.
- When the topic or phase is complete, run the relevant checks, push the branch,
  and open a PR targeting `main`. Describe the resulting behavior, scope, and
  validation so a developer can review it.
- Leave the PR open for developer review. Address requested changes on the same
  branch, rerun relevant checks, and wait for approval of the updated changes.
  Do not treat silence or automated checks as developer approval.
- Once the developer approves the current changes, merge the PR into `main`
  without asking for redundant confirmation, provided required checks pass and
  there are no unresolved review requests or merge conflicts. Do not bypass
  branch protection or required checks.
- After merging, update local `main` and delete the completed work branch both
  remotely and locally. Preserve any unrelated uncommitted work. Report the PR,
  merge result, and branch cleanup; if blocked, report what remains.
- Small standalone work that is not part of a topic or phase and fits in one
  focused commit may go directly to `main`, subject to the same validation and
  commit-message rules. A topic or phase still requires a branch and PR even if
  its implementation happens to fit in one commit.

## Commit messages

- Use Conventional Commits: `<type>(<optional scope>): <description>`.
- Choose the type from the change: `docs` for documentation, `fix` for bugs,
  `feat` for features, `refactor` for restructuring, `build` for build/toolchain
  changes, `ci` for workflows, `test` for tests, and `chore` for maintenance.
- Write the description in English, using an imperative verb, without a trailing
  period. Keep the subject at most 72 characters. Use a scope only when useful,
  such as `farm`, `save`, or `toolchain`.
- Keep each commit focused on one logical change. For substantial changes, add a
  blank line and a body explaining why; wrap body lines at 72 characters.
- Use `!` and a `BREAKING CHANGE:` footer for an actual breaking interface or
  workflow change. A matching-hash failure alone is not a Conventional Commits
  breaking-change declaration.
- Example: `docs: add agent guidelines and project onboarding guides`.

## Validation and reporting

- For C, linker, SDK-processing, or build-tool changes, run the documented Docker
  build and `cmake --build <build-dir> --target check`.
- The default check must report `all 14 files match the original`. With
  `ORIGINAL_DISC`, it also checks archive/disc outputs against their reference hashes.
- On Windows, use the Linux-copy procedure in `docs/building.md` if direct mounts
  fail. Normalize CRLF only in the disposable build copy.
- For documentation-only edits, check commands, paths, links, and `git diff --check`;
  a full binary rebuild is unnecessary unless build behavior also changed.
- State what was actually verified. Hash matching does not establish emulator
  gameplay testing, and code-only validation does not validate a full disc image.
