# VanillaBTN: project rules

VanillaBTN aims to run *Harvest Moon: Back to Nature* (PS1, USA) natively, using the
`hmbtn-decomp` matching decompilation as its base. The repository has a matching PS1
build and a Windows x64 startup executable.

These rules bind every contributor and every AI agent. `CLAUDE.md` and `GEMINI.md`
import this file. If a rule is unclear or conflicts with a task, stop and ask.

## 1. Principles

- **Karpathy:** state assumptions, do not guess, touch only what the task needs, define a
  check and verify it.
- **Ponytail:** the smallest complete change. No speculative options, wrappers or
  dependencies. Reuse what the codebase already has.
- **No hallucination:** check every API, flag and path against the installed headers,
  docs or a real run. If unverified, say "not verified".
- **Do not break what works:** the byte-matching PS1 build must keep passing (see 4).
- **Whole game, not one screen:** fix causes in shared code. No per-scene hacks.
  Use `ROADMAP.md` for implementation status and run targeted checks for each change.
  Extend long-play verification alongside development.

## 2. Code

- Format with `.clang-format` (4 spaces, K&R braces, 100 columns). Format only the lines
  you change. **Never mass-reformat** existing files: it buries history and the style is
  not uniform (a sampled `title.c` differs from the config on over half its lines).
- Names say what a thing is. Named constants, no magic numbers. Comments say why, in one line.
- No dead code, no commented-out code, no new warnings.
- New non-trivial logic gets one small test or assert-based check.
- A shortcut with a known limit: `/* shortcut: <limit>, <when to upgrade> */`.
- Do not touch `tools/maspsx` (upstream, own license).

## 3. Debug and release (native build)

The native port must provide Debug and Release configurations, selected by CMake
build type and preset:

| | Debug | Release |
|---|---|---|
| Optimization | `-O0 -g` | `-O2`, `NDEBUG`, LTO |
| Debug checks | assertions, ASan + UBSan where supported, `-Wall -Wextra` | disabled |
| Define | `VANILLABTN_DEBUG` | none |
| Log | `VanillaBTN.log` next to the exe, rewritten each run, flushed per line | no log file |

- Use the compiler's equivalent optimization, debug, and warning flags on each platform.
- Log lines: timestamp, level, module, message. Replace the home directory with `~` before
  writing. Never log secrets or personal data.
- Keep asset and save input validation enabled in both configurations.
- The PS1 matching build has fixed flags (`-O2 -gcoff`). It is not a debug/release build;
  never change those flags.

## 4. Verification

- Run the PS1 matching build locally in Docker before committing changes under `src/`,
  `cmake/`, `tools/`, `sdk/` or `disc/`. Use `tools/docker_build.sh` (see README). Expect
  `all 17 files match` with a disc, or the 14 code binaries without one.
- **Never trigger CI from a local machine** (no `act`, `gh workflow run`, `gh run rerun`).
  Keep the workflow manual-only (`workflow_dispatch`). Never add `push` or
  `pull_request` triggers.
- Smoke-test native changes on the host before and after. Report what you could not test.

## 5. Git workflow

- **Commits:** atomic (one concern, builds, passes checks) and
  [Conventional Commits](https://www.conventionalcommits.org): `type(scope): subject`.
  Types: `feat fix refactor perf docs build chore test style`. Imperative, lowercase,
  no period, 72 characters max. The body says why, only when needed.
- **Small work (1-2 commits):** commit straight to `main` and push.
- **Large work (3+ commits or risky):** branch `type/short-name` from `main`, push there,
  and **do not open a PR** until the developer asks. If the developer is not satisfied,
  keep working on the branch.
- **PR:** the developer reviews, approves and merges. The agent never merges or approves.
- **After merge:** the agent deletes the branch locally and on the remote, then
  `git fetch --prune`. Only `main` remains.
- Never force-push `main`. Never rewrite published history. Keep `main` buildable.

## 6. Privacy and safety

- Never commit or print secrets, `.env*`, tokens, keys, or game data (`*.bin`, `*.cue`,
  extracted files). Never expose private info.
- Write paths as placeholders: `$HOME`, `~/`, `%USERPROFILE%`, `$env:USERPROFILE`. Never
  `/home/<name>`, `/Users/<name>` or `C:\Users\<name>`.
- Before each commit, scan the diff for those patterns, emails and tokens.

## 7. Docs

- Short: bullets and tables, no paragraph over 3 lines. Say what to do, not the history.
- Update the docs that your change makes wrong, in the same commit.

## 8. License

MIT (`LICENSE`). Upstream and third-party terms are in `NOTICE`. Add a line there when you
add a third-party component. The game and the PSY-Q SDK are not ours and are not included.

## 9. Agent replies

- Plan first for multi-step work: `step -> check`.
- End with one or two lines: what you skipped or did not verify, and any risk.
