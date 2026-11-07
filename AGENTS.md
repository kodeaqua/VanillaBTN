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
