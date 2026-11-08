# Repository guidance

This repository is a matching decompilation of *Harvest Moon: Back to Nature*
(PlayStation USA, `SLUS_011.15`). The primary requirement for code changes is
that the generated EXE and 13 overlays still match the reference SHA-1 hashes.

## Read first

- [README.md](README.md) describes the project and its exceptions.
- [docs/architecture.md](docs/architecture.md) maps the runtime and build layout.
- [docs/building.md](docs/building.md) gives the build and verification commands.

## Working rules

- Keep changes scoped to the requested behavior or documentation. Do not
  reformat unrelated C files: source order, declarations, compiler behavior,
  object layout, and link order can affect the output bytes.
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

## Commits

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

## Windows and Docker

The old PS1 preprocessor can fail on a Windows bind mount and on CRLF source
files. Use `tools/build-in-container.sh` through Docker as documented in
`docs/building.md`; it copies sources to the container filesystem and converts
line endings in that temporary copy. It writes verified binaries to `build/`,
which is gitignored. Keep the original working tree untouched.
