# Architecture guide

## Runtime

The program targets the original PlayStation's MIPS R3000 environment and uses
PSY-Q libraries. It is not a native Windows game or a portable engine port.

`src/main/main.c` initializes CD access, graphics, sound, controllers, memory card
services, and shared state. It starts at `SCENE_TITLE`, loads the selected scene's
overlay from `A_FILE`, and installs `scene_frame_handler`. The frame loop handles
input, scene execution, drawing, and synchronization with VSync.

The executable remains resident while overlay code shares designated memory
regions. Root `CMakeLists.txt` defines `low` at `0x800B7E20` and `high` at
`0x8012DDE8`; save/load and the debug window use the high region. Inspect scene
dispatch and local overlay declarations before changing load behavior.

## Main gameplay

The largest gameplay area is `src/overlay/11_GF_FARM/`:

| Entry or directory | Responsibility |
| --- | --- |
| `farm_main.c` | Entry dispatch, startup, and event/scene transition state machine |
| `farm_frame.c` | Per-frame mode dispatch and ordered subsystem updates |
| `farm_load.c` | Farm resource loading |
| `people/` | Player, villagers, family, and harvest sprites |
| `animals/` | Livestock, chickens, dog, horse, and animal actions |
| `field/` | Field, greenhouse, mine, pond, fishing, and item requests |
| `world/` | Clock, calendar events, weather, scenes, doors, and camera view |
| `event/` | Event scripts, commands, conditions, messages, and milestones |
| `menu/` | Inventory/storage, shops, status pages, calendar, and house menus |

Typical execution flows from `farm_enter_dispatch()` through the handler selected
by `game_state.progress.entryStep`. During normal gameplay, `farm_mode_tick()`
runs `farm_run_frame_subsystems()`, steps the event state machine, and applies
pending mode changes. Subsystem ordering and scratchpad stack usage are explicit
parts of the implementation.

`src/main/game/farm_state.h` defines the shared `game_state` block and `farm_area`
working buffers. `src/include/game_types.h` contains smaller shared structures.
Consult the owning headers for field meanings and exact widths before editing
save-related or gameplay state.

Other overlays provide debug/PDA screens, save/load, five minigames, title,
credits, a debug window, and memory card management. Their local
`CMakeLists.txt` files declare source order and overlay placement.

## Build pipeline

The build uses CMake/Ninja to orchestrate custom tools rather than a host C compiler:

1. GCC 2.95.2 `cpp` preprocesses C and `cc1` emits assembly.
2. `maspsx` rewrites assembly to reproduce ASPSX behavior.
3. `tools/psx_bss.py` reproduces common/BSS layout; GNU MIPS `as` emits objects.
4. SDK conversion tools prepare the required PSY-Q objects; `sdk/patches.txt`
   describes one historical SDK-word correction.
5. Generated linker scripts link the executable and overlays together.
6. Packaging tools emit PS-X EXE/overlay files, then `check` compares SHA-1 hashes
   against `disc/binaries.sha1`.

With `ORIGINAL_DISC`, tools extract original assets, repack `A_FILE`, and rebuild
the disc using `disc/disc.xml`. These outputs are checked using `disc/disc.sha1`.
The repository does not contain the game's assets or the Sony SDK.

## Where to start for a change

Trace the scene from `src/main/main.c`, find its overlay entry handler, then follow
the relevant subsystem and shared state. Inspect local CMake declarations before
moving code or data. Binary matching is sensitive to layout and generated assembly,
so validate behavior-preserving changes with the matching build.
