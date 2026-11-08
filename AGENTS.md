# Repository guidance

VanillaBTN aims to bring *Harvest Moon: Back to Nature* to native Windows while
preserving the original experience. Today this repository is a matching PS1
decompilation, not a native port. Its EXE and 13 overlays form the reference
baseline and must continue to match their original SHA-1 hashes.

## Read first

- [README.md](README.md) describes the project and its exceptions.
- [docs/architecture.md](docs/architecture.md) maps the runtime and build layout.
- [docs/building.md](docs/building.md) gives the build and verification commands.
- [docs/porting.md](docs/porting.md) states the Windows port direction and fidelity rules.
- [docs/versioning.md](docs/versioning.md) defines the project's version increments.

## Engineering approach

Adapted from the [Karpathy-inspired coding guidelines](https://github.com/multica-ai/andrej-karpathy-skills/blob/main/CLAUDE.md)
for this repository's agent workflow:

- Before coding, identify assumptions, meaningful ambiguity, and tradeoffs.
  Ask for clarification when the answer changes the intended result; continue
  independent work while waiting. Recommend a simpler approach when warranted.
- Implement only what the task needs. Avoid speculative features, one-use
  abstractions, and configuration or error handling without a real use case.
- Change only files and lines that serve the task. Follow the existing style,
  remove code made unused by the change, and report unrelated issues without
  silently refactoring them.
- Define a verifiable result for multi-step work, share a brief plan, and
  iterate until the relevant checks pass. Use a regression test when it proves
  a bug or protects a meaningful behavior; use the binary `check` target for
  changes that can affect matching output.

## Working rules

- Keep changes scoped to the requested behavior or documentation. Do not
  reformat unrelated C files: source order, declarations, compiler behavior,
  object layout, and link order can affect the output bytes.
- Keep native Windows port work separate from the PS1 matching build. Preserve
  the reference hashes as a guardrail; do not weaken them to make port code fit.
  Default port behavior should retain the original game's mechanics, pacing,
  content, and presentation. Put optional modifications outside that baseline.
- Document new or changed code so its purpose and non-obvious behavior are
  clear. Put concise comments near tricky logic and constraints; use separate
  documentation for broader workflows or architecture, and update it when the
  code changes. Explain why the code works this way rather than restating it.
- Format new and modified code consistently with the surrounding style. Keep
  indentation, spacing, and line breaks tidy, and limit formatting changes to
  the code being worked on. Verify that C formatting changes preserve the
  required binary match with the `check` target.
- Each image lists its C files in link order in `src/main/CMakeLists.txt` or its
  `src/overlay/*/CMakeLists.txt`. Update these lists deliberately if files move.
- The EXE lives in `src/main`; scene code lives in `src/overlay`. Shared game
  state is declared in `src/main/game/farm_state.h`.
- `disc/binaries.sha1` covers the EXE and overlays. `disc/disc.sha1` additionally
  covers `A_FILE.BIN`, `A_FILE.HDT`, and `disc.bin` when an original disc is supplied.
- Run the `check` target after C, linker, SDK, or disc tooling changes. A
  successful compile alone is not sufficient. Report whether the 14-file code
  check or the full 17-file disc check ran.
- Do not add original game data, disc images, extracted assets, or PSY-Q SDK
  files to the repository. The project's own work is CC0; `tools/maspsx` has
  its own license.

## Native Windows build profiles

- Provide separate `Debug` and `Release` profiles when adding the native port.
  These profiles do not replace the fixed, byte-matching PS1 reference build.
- Favor fast compilation and useful symbols in `Debug`. Developer logs,
  prints, traces, assertions, and debug UI may run only in this profile.
- Optimize `Release` for runtime performance. Compile developer diagnostics
  out of the Release executable; hiding their output at runtime is not enough.
  Do not let diagnostic expressions carry gameplay side effects.
- Verify both profiles when native code exists: Debug must support diagnosis,
  and Release must run without developer diagnostics while preserving the same
  gameplay behavior and timing requirements.

## Commits

- Follow [docs/versioning.md](docs/versioning.md) for `MAJOR.MINOR.PATCH`.
  The developer alone chooses MAJOR; never change `VERSION_MAJOR` without that
  decision. Mark the final commit of a completed larger feature with
  `Feature-Complete: true` as its last message line. Other atomic commits
  advance PATCH automatically through `tools/version.py`.
- Use Conventional Commits for every new commit: `type(scope): summary` or
  `type: summary` when a scope adds no value. Write the summary in the
  imperative, for example `docs: clarify the Windows build steps`.
- Use a type that describes the change, such as `feat`, `fix`, `docs`, `refactor`,
  `test`, `build`, `ci`, or `chore`. Keep each commit focused on one logical change.
- Explain why in the commit body when the reason is not obvious. Mark breaking
  changes with `!` after the type or scope and a `BREAKING CHANGE:` footer.

## Workflow

- Finish each requested task with an atomic commit. Split independent work into
  separate commits so every commit has one clear purpose. Do not leave a
  completed task uncommitted.
- Small, self-contained work without a larger topic may be committed directly
  to `main`.
- For large or major changes, create a dedicated work branch. Make atomic
  commits on that branch, run the relevant checks, and open a pull request for
  developer review.
- Wait for the developer to review and explicitly instruct the agent to merge
  an approved pull request. Do not merge based on an assumed approval.
- After the pull request is merged, delete its work branch locally and on the
  remote. Keep `main` checked out and up to date.

## Sensitive information

- Never write, commit, upload, or publish secrets or private data, including
  `.env` files, credentials, tokens, private keys, personal information,
  machine-specific home paths, original game data, and SDK files. Keep them out
  of code, documentation, commit messages, pull requests, and shared artifacts.
- Use repository-relative paths or portable placeholders in examples and
  reports. Write `~/...` or `$HOME/...` on Unix, `%USERPROFILE%\...` in Windows
  Command Prompt, and `$env:USERPROFILE\...` in PowerShell. Do not include a
  real username or home directory in a path that will be shared.
- Inspect staged changes and artifacts before committing, pushing, or
  uploading. Remove or redact sensitive values before sharing them, and do not
  print their contents while checking.

## Windows and Docker

The old PS1 preprocessor can fail on a Windows bind mount and on CRLF source
files. Use `tools/build-in-container.sh` through Docker as documented in
`docs/building.md`; it copies sources to the container filesystem and converts
line endings in that temporary copy. It writes verified binaries to `build/`,
which is gitignored. Keep the original working tree untouched.
