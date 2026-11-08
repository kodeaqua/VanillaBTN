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

## Windows and Docker

The old PS1 preprocessor can fail on a Windows bind mount and on CRLF source
files. Use `tools/build-in-container.sh` through Docker as documented in
`docs/building.md`; it copies sources to the container filesystem and converts
line endings in that temporary copy. It writes verified binaries to `build/`,
which is gitignored. Keep the original working tree untouched.
