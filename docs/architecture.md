# Architecture

VanillaBTN's current codebase is the matching PS1 reference build. There is no
native Windows runtime yet. See [porting.md](porting.md) for the intended port
direction; the layout below describes what exists today.

## PS1 runtime

`src/main/main.c` initializes the PlayStation libraries, graphics buffers,
input, sound, and persistent game state. Its outer loop chooses a scene, reads
the corresponding overlay from `A_FILE`, installs a scene handler, and runs a
frame loop. `main_request_scene()` ends the current frame loop and selects the
next scene. `src/main/overlay_regions.c` declares the low and high load regions.

The EXE's reusable systems are grouped under `src/main`: `cd` reads `A_FILE`,
`game` owns shared state and tables, `geometry` and `graphics` render scenes,
`input` reads controllers, and `sound`, `text`, and `ui` serve scene code.

`src/overlay/11_GF_FARM` is the main gameplay overlay. `farm_main.c` dispatches
entry and event modes. Its modules cover animals, events, field interactions,
menus, people, and the world. `src/main/game/farm_state.h` declares the shared
`game_state` and farm load state used across the EXE and overlays. Other
overlays implement the title, save/load, five minigames, staff roll, memory
card screen, and debug or sample screens. `src/main/main.c` is the authoritative
map from scene IDs to overlay files and entry handlers.

## PS1 build

`CMakeLists.txt` declares the EXE, 13 overlays, two overlay load regions, and
the `check` target. Each image's `CMakeLists.txt` lists source files in link
order. `cmake/psx.cmake` compiles objects and generates one linker script for
the EXE and overlays. `tools/psx_cc.py` runs the old `cpp` and `cc1`, then
`maspsx`, `psx_bss.py`, GNU assembler, and `objcopy`. The Dockerfile provisions
those tools and the PSY-Q 4.7 SDK.

`disc/a_file.txt` identifies which `A_FILE` members are built here and which
must be extracted from the user's original disc. With `ORIGINAL_DISC`,
`cmake/disc.cmake` extracts the remaining data, repacks `A_FILE.BIN` and
`A_FILE.HDT`, then builds `disc.bin` and `disc.cue` using `disc/disc.xml`.
Without it, the build produces the 14 code binaries only.

`check` compares generated files with `disc/binaries.sha1` and, for a full
disc build, `disc/disc.sha1`. CI runs the 14-file check on pushes and pull
requests. See [building.md](building.md) for commands and platform notes.

The port can reuse the understanding and data structures gained here, but its
Windows platform code should be separate so the PS1 reference continues to
build and match independently.

## Where to start

| Task | First files to read |
| --- | --- |
| Scene transitions | `src/main/main.c`, `src/main/main.h` |
| Farm simulation | `src/overlay/11_GF_FARM/farm_main.c`, `src/main/game/farm_state.h` |
| Disc reads and overlay loading | `src/main/cd/afile.c`, `disc/a_file.txt` |
| Matching and link layout | `src/main/CMakeLists.txt`, `src/overlay/*/CMakeLists.txt`, `cmake/psx.cmake` |
| Disc assembly | `cmake/disc.cmake`, `tools/psx_disc.py` |
