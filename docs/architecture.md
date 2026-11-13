# Architecture

VanillaBTN has a matching PS1 reference build and an incomplete native Windows
port that runs the original startup and scene dispatcher. SDL supplies the
window, device input, presentation and audio output; SDK bridges supply the
platform services. Imported MIPS overlay bytes are data, never executed. See
[Work 01](01-porting.md) for the baseline port direction and
roadmap, and [PS1 to Windows boundaries](#ps1-to-windows-boundaries) below for the
service map. The PS1 sections come first and describe the PS1 source.

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

## PS1 to Windows boundaries

The matching PS1 source remains the reference. The native target reuses
game rules and data formats while replacing these host-facing services:

| Boundary | PS1 source and behavior | Native direction |
| --- | --- | --- |
| Startup, scenes, timing | `src/main/main.c` initializes PSY-Q, uses VSync, loads one MIPS overlay into a fixed region, and runs its scene handler. `src/main/overlay_regions.c` identifies two regions. | Keep scene handlers as native modules. Select a handler by scene ID; never execute imported MIPS overlays or treat PS1 addresses as host pointers. Preserve the game's simulation cadence. |
| Disc and archive | `src/main/cd/afile.c` finds `A_FILE.BIN` and `A_FILE.HDT` through `CdSearchFile`, uses 2048-byte sectors, and has synchronous and polled reads. `src/main/cd/cd_audio.c` reads XA streams. | Read imported files by byte offset. Preserve archive member indices and read completion semantics where callers depend on them. Decode XA separately for audio. |
| Graphics | `src/main/graphics`, `src/main/geometry`, and `main.c` use GPU primitives, ordering tables, GTE, VRAM/CLUT, and scene-specific display modes. | The native software GPU holds 1024x512 16-bit VRAM words and implements fills, tiles, sprites, flat and textured polygons, ordering tables, and a GTE layer. The original game never requests a texture window (its only `SetDrawMode` call passes none), so the SDK bridge rejects a non-empty window explicitly instead of ignoring it. |
| Input | `src/main/input/pad.c` polls two PS1 pads; `pad_vibration.c` drives their actuators. | Map SDL3 input and rumble to the existing pad records and polling rules. |
| Sound | `src/main/sound/sound.c` uses SPU/VAB/SEQ; `cd_audio.c` controls XA. | Decode and mix game audio to PCM, then submit it through SDL3. |
| Save/load | `src/overlay/15_saveload/saveload.c` and `src/overlay/31_gf_mcard` use PS1 card operations. | Preserve the original save-file bytes and card UI behavior with virtual slots. |

The PS1 build assumes 32-bit addresses and uses fixed regions such as the
frame-work pointer in `src/main/main.c`. The native x64 process must keep
serialized PS1 addresses as explicit 32-bit values and translate them before
access; casting them directly to pointers is invalid. The initial native
target already checks that its host pointers are 64-bit.

### Data handoff

`tools/import-game.ps1` composes the existing `psx_disc.py` and `afile.py`
extractors on Windows. It produces a disc-file tree and 32 unpacked archive
members under `build/game-data/` by default. `disc/a_file.txt` preserves the
member order and original names; `A_FILE.HDT` is 33 little-endian 32-bit byte
offsets into `A_FILE.BIN`, including the terminal offset. Every offset is on
a 2048-byte boundary. The importer validates these bounds before writing
members. Imported overlays are PS1 code and are not native scene modules.

The importer uses Python only during development. The native executable must
read prepared data without Python, and a standalone import path is needed
before distribution. Imported files remain local and must never be committed.

### Graphics slice

The first original graphics module now compiles against native SDK transfers;
see [Native SDK boundary](native-sdk.md). Original scene code submits its
primitives through the SDK bridge in the host loop.

`native/src/ps1_video.c` implements the tested GP0(02) quick fill and
GP0(60/62/68/6A) untextured tile rules: 8-to-5-bit color conversion, fill
rounding and VRAM wrapping, draw-area clipping, 11-bit coordinates, mask bits,
and four 5-bit transparency modes. These rules follow the
[PSX-SPX GPU specification](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/).
The raw texel sampler decodes 4-bit and 8-bit CLUT indexes or a direct 15-bit
word while preserving zero and bit 15.
The sprite path now draws raw or modulated texels, skips color zero, blends
only texels with bit 15 when semitransparency is requested, and applies the
drawing area's clipping and mask rules. Software triangle and quad rasterization
covers flat and textured polygons (POLY_F3, POLY_FT3, POLY_F4, POLY_FT4).
Texture windows are unimplemented because the original game does not use them; the SDK bridge rejects a non-empty window. The game mirrors sprites by swapping UVs in `anim.c`, not through GPU flip bits. CLUT cache timing is not modeled; it affects hardware timing, not pixels.
Original scenes issue these commands through the native SDK bridge; fidelity of
the result is not established by synthetic tests.

`native/src/ps1_ot.c` stores packet IDs behind host-sized links rather than
the PS1's 24-bit MIPS addresses. A table walks buckets from high to low and
visits newer packets first within one bucket, matching `ClearOTagR` and
`addPrim`. `ps1_gpu_submit_ot` and `ps1_gpu_submit_frame` walk single or multiple
ordering tables, unpack each packet tag's length, and route GP0 command words
into the dispatcher, supporting the game's table sequence (OT3, OT2, main OT,
OT1, OT0, active OT).

`native/src/ps1_gte.c` provides the host-side GTE compatibility layer: fixed-point
vector rotation and translation, perspective divide (RTPS/RTPT), normal clipping
(NCLIP), and depth averaging (AVSZ3/AVSZ4).

### Input and clock

`native/src/ps1_input.c` emulates the `InputRecord` semantics from `pad.c`:
`held`, `pressed`, and dual auto-repeat channels (`triggered` and `triggered2`).
The original game loop is paced by `native/src/sdk_sync.c`, which emits 60 Hz
VBlank ticks from the host clock (multiplied by the optional turbo setting) and
runs the game's VSync callback. `native/src/ps1_clock.c` is a separate
standalone helper that accumulates host nanoseconds into nominal 30 Hz ticks
with an anti-cascade clamp; the running game does not pace itself with it.
Exact NTSC hardware drift remains unmodeled. `native/src/main.c` maps SDL3
keyboard (WASD/arrows, ZXCV, Enter/Space/Esc) and SDL3 Gamepad events directly
to PS1 controller bits for the original `pad.c` logic.

### Audio slice

`native/src/ps1_spu.c` provides the host-side SPU mixer (24 voices as on hardware; an optional
expansion to 64 voices is described in [Work 02](02-enhancements.md)):
- Decodes 16-byte PS1 ADPCM blocks with the 5 hardware IIR prediction filters
  and loop start/repeat/end semantics.
- Parses Sony PSY-Q VAB headers (magic `pBAV`), 128-program ProgAtr tables,
  VagAtr tone entries, and VAG offset tables.
- Computes pitch step from semitone difference against the tone's center note
  and fine cents, with linear interpolation on resampling.
- Supports independent voice volume and master volume scaling.

`native/src/ps1_xa.c` implements the CD-ROM XA streaming audio player:
- Filters 2336-byte Form 2 disc sectors by file number and channel.
- Decodes CD-XA ADPCM (18 sound groups per sector, 224 samples per group) with
  filter history and sample clamping.
- Implements the 26 CD audio tracks and volume fading curves matching `cd_audio.c`.

`native/src/ps1_audio.c` bridges SPU mixing and CD-XA playback into SDL3's
`SDL_AudioStream` at 44 100 Hz stereo 16-bit signed PCM, with headless fallback
when no audio output hardware is present. `ps1_audio_tick` is its standalone
30 Hz path (1470 frames per tick). In the running game, `native/src/main.c`,
through `sdk_sound_mix`, mixes 735 stereo frames per 60 Hz VBlank tick, derives
PCM from elapsed time when a tick is late (capped at four ticks), keeps the SDL
queue within four ticks and still produces one tick of audio per turbo tick;
see [Work 02](02-enhancements.md#native-sdl-music-onset-buffering).

### Save and memory-card slice

`native/src/ps1_card.c` implements two virtual PS1 memory card slots:
- Models standard 128 KB (131 072 bytes) raw PS1 memory card images (.mcr / .mcd):
  Frame 0 header with `MC` magic and XOR checksum, and 15 directory frames with
  block allocation chains (0x51 initial, 0x52 middle, 0x53 last, 0xA0 free).
- Emulates the Harvest Moon: Back to Nature 4-block save files (`BASLUS-01115-FARM0`
  through `FARM2`, 32 768 bytes each) and the 128-byte slot header record at
  offset 0x200 with 1-byte XOR checksum validation.
- Employs atomic write protection: writes to `<file>.tmp`, validates the image,
  backs up the previous card to `<file>.bak`, and replaces the target.
- Automatically recovers from interrupted writes or sudden power loss if a valid
  `.tmp` or `.bak` file remains.
- Translates live x64 `game_state` structures (`FarmAnimals`, `FarmPlayer`,
  `FarmHelper`, `FarmAction`, `FarmPet`, `AnimState`, `ItemCue`) to and from
  byte-accurate PS1 32-bit wire layouts (`SaveFile`, `SaveImage`, `Ps1AnimalsWire`,
  `Ps1PlayerWire`, `Ps1HelperWire`) via `save_pack_farm_state()` and
  `save_unpack_farm_state()`, with compile-time assertions on all wire sizes and offsets.
- Serializes `game_state.request` pointer relative to PS1 base address `0x800742B0`
  into a 32-bit address word, deserializing back to the corresponding
  `FarmRequestSlot` in native memory.
- Clears transient sprite animation pointers (`source`, `bank`, etc.) and zeroes
  wire padding bytes to ensure deterministic, console-compatible save images.
- Stores card files under SDL3's per-user preferences directory (`SDL_GetPrefPath`).
