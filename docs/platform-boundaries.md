# PS1 to Windows boundaries

The matching PS1 source remains the reference. The native target will reuse
game rules and data formats while replacing these host-facing services:

| Boundary | PS1 source and behavior | Native direction |
| --- | --- | --- |
| Startup, scenes, timing | `src/main/main.c` initializes PSY-Q, uses VSync, loads one MIPS overlay into a fixed region, and runs its scene handler. `src/main/overlay_regions.c` identifies two regions. | Keep scene handlers as native modules. Select a handler by scene ID; never execute imported MIPS overlays or treat PS1 addresses as host pointers. Preserve the game's simulation cadence. |
| Disc and archive | `src/main/cd/afile.c` finds `A_FILE.BIN` and `A_FILE.HDT` through `CdSearchFile`, uses 2048-byte sectors, and has synchronous and polled reads. `src/main/cd/cd_audio.c` reads XA streams. | Read imported files by byte offset. Preserve archive member indices and read completion semantics where callers depend on them. Decode XA separately for audio. |
| Graphics | `src/main/graphics`, `src/main/geometry`, and `main.c` use GPU primitives, ordering tables, GTE, VRAM/CLUT, and scene-specific display modes. | The native slice holds 1024x512 16-bit VRAM words, presents a 320x240 rectangle, handles quick fill, tiles, and basic textured sprites through TPage/CLUT. Texture windows, textured polygons, ordering, GTE, and scene modes still need implementation. |
| Input | `src/main/input/pad.c` polls two PS1 pads; `pad_vibration.c` drives their actuators. | Map SDL3 input and rumble to the existing pad records and polling rules. |
| Sound | `src/main/sound/sound.c` uses SPU/VAB/SEQ; `cd_audio.c` controls XA. | Decode and mix game audio to PCM, then submit it through SDL3. |
| Save/load | `src/overlay/15_saveload/saveload.c` and `src/overlay/31_gf_mcard` use PS1 card operations. | Preserve the original save-file bytes and card UI behavior with virtual slots. |

The PS1 build assumes 32-bit addresses and uses fixed regions such as the
frame-work pointer in `src/main/main.c`. The native x64 process must keep
serialized PS1 addresses as explicit 32-bit values and translate them before
access; casting them directly to pointers is invalid. The initial native
target already checks that its host pointers are 64-bit.

## Data handoff

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

## Graphics slice

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
Texture windows, flips, and CLUT cache timing remain pending.
No game scene issues these commands through the native target yet.

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

## Input and clock

`native/src/ps1_input.c` emulates the `InputRecord` semantics from `pad.c`:
`held`, `pressed`, and dual auto-repeat channels (`triggered` and `triggered2`).
`native/src/ps1_clock.c` accumulates host nanoseconds and emits exact 60 Hz
simulation ticks with an anti-cascade clamp. `native/src/main.c` maps SDL3
keyboard (WASD/arrows, ZXCV, Enter/Space/Esc) and SDL3 Gamepad events directly
to PS1 controller bits. Scene wiring remains next.

## Audio slice

`native/src/ps1_spu.c` provides the host-side 24-voice SPU mixer:
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
when no audio output hardware is present. `native/src/main.c` advances audio
each 60 Hz simulation tick.
