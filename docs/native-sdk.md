# Native SDK boundary

The native target compiles `src/main/graphics/texture_pack.c` directly into
`game_texture_pack`. It does not copy its decoding or placement rules into a
replacement loader. Native declarations under `native/compat` and the
`sdk_gpu` implementation supply the currently supported platform boundary.
These are project-owned declarations, not bundled proprietary SDK headers.

`LoadImage` writes packed 16-bit pixels to the host VRAM; `StoreImage` reads
them back. Row widths are halfwords, even though the API pointer is word-typed.
Transfers finish synchronously, so `DrawSync(0)` and `DrawSync(1)` report an
empty queue. There are no callbacks or asynchronous GPU commands yet. The
entry-point signatures and transfer semantics follow the
[SDK runtime reference](https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf).

The host binds its VRAM with `sdk_gpu_bind` and unbinds it before shutdown.
There is one GPU context, used on the host thread. Invalid transfer rectangles
return -1 before modifying VRAM. GPU drawing environments do not affect image
transfers. No rendering, animation, or game state machine is added here.

MSVC rejects arrays of structures containing zero-length trailing arrays.
`VANILLABTN_NATIVE` therefore uses one-element trailing placeholders in the
texture bank declarations. Static checks preserve the image offset (4 bytes)
and pixel payload offset (12 bytes). File iteration uses `BANK_NEXT`, never
native `sizeof(ImageBank)` or `sizeof(BankImage)`. PS1 declarations stay intact.

Unused private original helpers reference animation code not compiled yet.
Function sections and dead-code elimination discard those unreachable helpers;
there are no pretend animation implementations. Primitive declarations preserve
the PS1 wire layout. Unsupported SDK operations remain build or link errors
rather than silent no-ops.

`original_texture_pack` tests real original loader calls with synthetic banks,
raw/RLE pixels, placement offsets and image-bank expansion. Transfer fixtures
also cover odd widths, unaligned buffers, boundary pixels, readback and invalid
rectangles. The game is still not connected to the main loop; this validates
the first original graphics module's platform boundary, not gameplay.

The original loader assumes valid banks, dimensions within its fixed row
buffer, and valid image indices. It does not accept a byte length or validate
malformed input; an absent image index can leave its result undefined. These
original preconditions are not relaxed by compiling it natively. A validated
asset handoff remains necessary before connecting imported data to gameplay.

## Original primitive submission

`game_prim` compiles the original `prim.c`. SDK packet declarations retain their
32-bit tags and GP0 payload fields; static fixtures check TILE/SPRT/FT4 sizes.
The host supplies bounded GS work storage through `sdk_work_bind`; the original
builders still own command construction, colors, coordinates and packet sizes.
Arena exhaustion stops execution instead of corrupting adjacent host memory.

Before building a frame, prefer registering complete packet arenas through
`sdk_gpu_map`. `ClearOTagR` maps module-owned OTs and `addPrim` lazily maps
private packets outside arenas, using trusted live SDK-call pointers. Partial
overlap cannot extend a declared arena's bounds. Serialized links are still
resolved only through registered spans. Tags use a 24-bit mapped offset, never
a truncated host pointer.
Mappings are disjoint and frame-local; reset them only after draws complete.
`ClearOTagR` links each OT entry to its predecessor. `addPrim` inserts at the
head of that entry; `DrawOTag` resolves links and submits packets to the existing
GPU dispatcher. The dispatcher still has incomplete primitive coverage.

Unmapped, unaligned and out-of-range links are rejected, and a traversal budget
terminates cycles. `sdk_gpu_ok` reports a sticky submission error until mapping
reset. The host must check it when wiring the original frame loop. Existing
single-GPU synchronous transfer and lifetime constraints still apply. No game
scene is connected by this step.

## Original animation code

`game_anim` compiles `src/main/graphics/anim.c` and `src/main/system.c`
directly. Byte-pointer offset arithmetic replaces the compiler-specific `void*`
addition. Native builds omit the farm-state include used only for PS1
return-address diagnostics; those MIPS addresses have no native meaning. No
playback or drawing rule is replaced. `LINK_PRIM` routes through the same mapped
SDK links as `addPrim`, preserving the original packet length and preventing
x64 pointer truncation.

Original fatal debugger waits terminate through `pollhost()` in the compiled
`host_poll` routine, while `display_on`/`display_off` drive `SetDispMask`.
Font diagnostic output exists only in Debug; Release discards calls and their
arguments at compile time. The native compatibility header does not supply
fake implementations for unported scratchpad or other CPU operations.

Synthetic fixtures invoke the actual original duration/hold/pause logic and
SPRT/FT4 generation, including UV orientation and primitive-cursor advancement.
They supply the main module's frame pointer as fixture state. Nonuniform texel
fixtures verify standard forward FT4, oriented-UV FT4, and SPRT subpixel
interpolation against console affine polygon rasterization behavior.

`game_frame_buffers` compiles the original frame layout, cursor insertion,
`src/main/geometry/math_util.c`, `src/main/geometry/camera.c`,
`src/main/geometry/vec_length.c`, `src/main/geometry/ground.c`,
`src/main/geometry/model.c`, `src/main/graphics/clut.c`,
`src/main/ui/loading_screen.c`, `src/main/ui/ui_icons.c` and
`src/main/ui/bg_tile.c`. Native insertion replaces
only the 24-bit pointer cast with SDK link mapping. `SetDrawLoad` constructs
inline `DR_LOAD` (`GP0 0xA0`) CPU-to-VRAM transfer packets while preserving any
link already stored in the tag's low 24 bits by `ot_insert_prim`, and
`ps1_gpu_exec` copies those halfwords into VRAM with bounds and word-count
validation. `sdk_math` connects `<libgte.h>`, `<inline_c.h>` and `<gtemac.h>`
entry points (`SetGeomOffset`, `SetGeomScreen`, `SetFogNear`, `SetFarColor`,
`PushMatrix`, `PopMatrix`, `RotMatrixX/Y/Z`, `ApplyMatrixLV`, `OuterProduct0`,
`gte_OuterProduct12`, `gte_MulMatrix0`, `VectorNormal`, `VectorNormalS`,
`VectorNormalSS`, `SquareRoot0`, `GsIDMATRIX`, and the `gte_ld*`/`gte_rt*`/
`gte_nclip`/`gte_avsz*`/`gte_sqr0`/`gte_op12`/`gte_st*` coprocessor calls) to
`ps1_gte`, while `ground.c` moves its two GCC nested helpers (`ground_triangle_under`
and `ground_quad_under`, which capture no local variables) to file scope under
`VANILLABTN_NATIVE` so MSVC C17 compiles the original height and triangle-hit
logic unchanged. The original OT dimensions, memory offsets, clearing, cursor
advancement, camera polar/frustum/view setup, TMD model projection/culling,
ground triangle height/collision lookup, CLUT palette blending/bouncing/wrapping,
loading-picture texture upload, double-buffered loading OT/packet slots,
environment flips, UI icon blink cycles/sprites and diagonal 24-pixel
background tiling remain authoritative. Tests allocate a sufficiently large
host region, map it once, invoke the original layout for both frames, submit
inserted packets, and exercise loading-screen, CLUT-fade/scroll, camera, UI
icon, background-tile, ground-height/collision and TMD model-projection/draw
execution. Frame storage is runtime-only; its host
pointer layout is never serialized as a PS1 frame record or saved game.

## SDK timing

`sdk_sync` supplies `VSync`: mode 0 waits for the next vblank, mode 1 queries
elapsed horizontal units without waiting, n greater than 1 waits until n
vblanks since the last completed wait, and negative modes query the absolute
vblank count. This follows the SDK runtime reference linked above. The initial
implementation uses nominal 60 Hz and 15,750 horizontal units per second;
exact NTSC drift, scanline position and interrupt callbacks remain unmodeled.

The default clock and absolute-deadline wait use SDL. Synthetic tests bind a
deterministic clock to exercise deadlines without sleeping. Deadlines retain
their phase and late work does not incur another full wait. Repeated
`ResetCallback` calls, as used by original disc reads, initialize the timing
source once without rewinding it. No interrupt callback registration API is
provided yet; this is not full BIOS callback emulation.

## Original archive reads, sound system and CD audio control

`game_afile` compiles `src/main/cd/afile.c`, `src/main/sound/cd_audio.c`,
`src/main/sound/sound.c`, `src/main/sound/sound_request.c` and
`src/main/game/event_flags.c` against `sdk_cd`, `sdk_sound` (`ps1_spu` and
`ps1_seq`) and `sdk_sync`.
The native-only initialization scratch is one sector instead of borrowing the
host farm-state object. It checks the 33-word HDT length, aligned monotonic
offsets and terminal BIN size before the original member table is used. In
`sound.c`, `sound_update` compares the 64-bit `seq_data[i]` pointer directly
against `SOUND_BUFFER_NONE` (`(u8 *)-1`) under `VANILLABTN_NATIVE` instead of
truncating it to 32-bit `s32`. PS1 initialization and its reference build retain
their original declarations and bytes.

`sdk_cd_mount` mounts the imported disc directory. `CdSearchFile` resolves
PS1-style names and assigns stable virtual LBAs for the mount; MSF positions
use BCD and the usual 150-sector lead-in. These addresses identify native file
extents, not the original physical disc layout or executable overlay memory.
`CdRead` records a pending request, `CdReadSync` completes it, and `CdReadBreak`
cancels it. Host reads are synchronous at completion: there are no artificial
busy frames or emulated seek delays. Sector padding after EOF is zero-filled;
requests cannot cross the selected file's allocated sector extent.

For `cd_audio.c`, `sdk_cd` additionally handles `CdlSetloc`, `CdlStandby`,
`CdlPause`, `CdlSetfilter`, `CdlSetmode` (`0xC8` for filtered real-time XA
ADPCM streaming in addition to `0x00`/`0x80` 2048-byte data modes), `CdlReadS`,
`CdlGetlocP`, `CdControlB`, `CdControlF` and `CdLastCom`. During active XA
streaming, `CdlGetlocP` advances the subheader Q position by 8 sectors per
query (one channel-interleaved cycle) across the selected track's sector extent
so `cd_audio_fade_tick` observes track progress, end-of-track stop and loop
seek behavior. `sdk_sound` provides `SsSetSerialVol` to record the CD serial
input left/right volume (`0..127`) adjusted by `cd_audio_play_track`,
`cd_audio_fade_tick` and `cd_audio_stop`, plus the `libsnd`/`libspu` entry
points used by `sound.c` and `sound_request.c`: `SsInit`, `SsSetTableSize`,
`SsSetTickMode`, `SsSetReservedVoice`, `SsStart`, `SsEnd`, `SsSetMVol`,
`SsSetRVol`, `SsSetStereo`, `SsSetMono`, `SsSetSerialAttr`, `SsVabOpenHead`,
`SsVabTransBodyPartly`, `SsVabTransCompleted`, `SsVabClose`, `SsSeqOpen`,
`SsSeqPlay`, `SsSeqPause`, `SsSeqReplay`, `SsSeqStop`, `SsSeqSetVol`,
`SsSeqClose`, `SsUtKeyOn`, `SsUtKeyOff`, `SsUtKeyOnV`, `SsUtKeyOffV`,
`SsUtAllKeyOff`, `SsUtSetVVol`, `SsVoKeyOff`, `SsUtReverbOn`, `SsUtReverbOff`,
`SsUtSetReverbType`, `SsUtSetReverbDepth`, `SsUtSetReverbFeedback`,
`SsUtSetReverbDelay`, `SpuSetCommonAttr`, `SpuGetAllKeysStatus` and
`SpuGetKeyStatus`. VAB headers and chunk-streamed bodies are copied into
per-slot SPU buffers and loaded into `Ps1Spu`, while opened `pQES` sequences
and keyed voices drive `Ps1Seq` and `Ps1Spu` directly. `Ps1Spu` implements voice
ADSR envelopes use 44.1 kHz shift/step counters, exponential modes, all-ones
rate holds and the original sustain targets, following the
[SPU envelope specification](https://psx-spx.consoledev.net/soundprocessingunitspu/).
A zero-valued VAB ADSR register starts the fastest attack; only the standalone raw
sample helper explicitly holds full envelope volume. Key-off starts release and
repeated key-offs do not abruptly silence a releasing voice. Numerical tests
cover envelope clock values; interpolation and full sound fidelity remain open.
The current circular delay-line effect is a generic echo, not validated PS1
reverb DSP; hardware reverb remains an open task. Starting an archive sector read via
`afile_read_bytes` invokes the real `cd_audio_stop` and switches the drive back
to data mode (`0x80`). Feeding `cd_audio.c` filter/location/volume state into
the live `ps1_xa` audio mixer stream remains a separate integration step.
File lookup failure is real failure: the host must validate the data tree before
entering original retry loops. Mount/unmount invalidates all pending requests.

The archive, sound and CD audio fixture exercises `xa_search_files`,
`cd_audio_play_track` (valid, out-of-range and event-flag muted tracks),
`cd_audio_fade_tick` (fade-out to stop, end-of-track stop and looped track
rewinds), `cd_audio_stop`, `event_flag_set`/`event_flag_get` bitfields,
`afile_read_bytes` stopping active XA playback, `sound_fader_init`, `sound_init`,
`sound_init_stereo_xa`, `sound_set_mono`/`sound_set_stereo`, `sound_fade_out`/
`sound_fade_in`, `sound_request(SOUND_REQUEST_RESET)` VAB chunk streaming via
`sound_load_tick`, `sound_play` + `sound_voice_set_volume` + `sound_release_voice`
ADPCM voice mixing, and `sound_queue_seq_pair` + `sound_update` +
`sound_seq_start_reverb` + `sound_seq_stop_first` + `sound_release_from`. It
also tests BCD conversion, missing files, traversal rejection, read
cancellation, padding, extent bounds and an oversized HDT rejected before it
can overflow native scratch.

## Original data regions

`sdk_memory` owns one aligned 2 MB main-RAM allocation. Serialized addresses
in the low, cached (`0x80000000`) and uncached (`0xA0000000`) segments resolve
to the same bounded storage. Out-of-range spans, unsupported segments and
scratchpad addresses return NULL. Additional physical RAM mirrors, BIOS,
scratchpad and device-register mappings are not implemented.

The original `overlay_regions.c` now has a native-only table using the same
LOW/HIGH data offsets as the reference linker configuration, within that one
allocation. Their pointer subtraction is valid on the host. PS1 builds retain
their linker-defined symbols. These are data destinations; native scene
functions still need separate linkage and dispatch. Imported MIPS code is
never executed or cast to a host function pointer. This does not yet connect
original global-state initialization, startup or the game loop.

## Display environments

`SetDefDrawEnv`/`PutDrawEnv` select clipping, draw offsets, texture page and
blend mode; `isbg` clears the selected area through the existing quick-fill
rules. `ClearImage` and `MoveImage` provide direct VRAM operations independent
of draw clipping. Moves currently snapshot the source, including overlaps;
hardware overlap ordering still needs independent fixtures.

`SetDefDispEnv`/`PutDispEnv` select the displayed VRAM rectangle and
`SetDispMask` controls visibility. SDL presents that rectangle and recreates
its nearest-neighbor texture when the original internal display size changes.
This supports original 320x240 and 640x480 buffers without a resolution upgrade.
The memory-only SDL software-renderer test verifies flips, masking and resize;
it opens no window and performs no gameplay or visual acceptance test.

RGB24 display and nonzero texture windows are explicitly rejected at this
boundary. Dithering, scanline/interlace timing, screen overscan placement and
drawing-to-displayed-area restrictions remain incomplete; accepting an
environment does not establish those hardware behaviors.

`game_screen` compiles the original `screen_fade.c`, `farm_state.c`,
`farm_tables.c`, `shop_restock.c`, `cooking_recipes.c`, `text_bank.c`,
`picture_codes.c`, `horse_names.c`, `farm_data_size.c`, `debug_stubs.c`,
`11_GF_FARM/farm_data.c` and `11_GF_FARM/farm_vars.c` unchanged (except
`main_get_region_base` returning a 64-bit `u8 *` under `VANILLABTN_NATIVE` so
overlay region base addition in `farm_state_init` and `menu_window.c` does not
truncate). The fixture provides main-loop frame globals and records/isolates
main control boundaries; fade counters, private packets, blend mode, shader
levels, recipe/picture/glyph tables, item/crop/price tables, `farm_state_init`,
`farm_tables_register`, `farm_work_sector_get`/`release` and horse-race runner
restocking come from the original code. Synthetic in/out frames verify resulting
VRAM values, and state/table/restock checks verify `farm_state_init` defaults,
item prices/seeds/crops, work-sector claims, and distinct runner selection. This
does not wire startup or replace its publisher/title sequence.

`game_window_frame` compiles the original bevel/gradient drawing routine
(`window_frame.c`), message-window and glyph-layout engine (`text.c`), field
menu box/dispatcher (`menu_window.c`), villager affection heart icon
(`heart.c`), name/birthday entry screen (`name_entry.c`), the `11_GF_FARM`
VRAM picture-cell allocator and storage screens (`picture_cells.c` and
`menu/item_storage.c`), the title overlay (`27_GF_TITLE/title.c`) and the
staff-roll overlay (`28_GF_STAFF/staff.c`). In `text_bank.h` and `picture_cells.c`,
`VANILLABTN_NATIVE` preserves full 64-bit host pointers in `LOCALIZED`,
`LOCALIZED_NAME` and `item_villager_reaction` (`game_state.pictureCodes`)
instead of truncating pointer arithmetic to 32-bit `u32`/`s32`. `libgpu.h`
exposes `AddPrim`, `setPolyF4`/`SetPolyF4`, `SetSemiTrans`, `setRGB0`,
`setXYWH` and `MoveImage2`, and `sdk_types.h` aliases `__builtin_memcpy` to
`memcpy` on MSVC.
`sdk_gpu` supplies `SetDrawMode`, `DR_AREA` and `SetDrawArea` (`GP0 0xE1..0xE4`
draw-mode and draw-area packets), `ps1_gpu_exec` dispatches multi-word
`0xE1..0xE6` environment packets, `libgs.h` exposes `HWD0`/`VWD0` (`320`/`240`),
and `sdk_work` supplies `SystemError` at the fatal platform boundary.
Its actual packets exposed missing flat-line and Gouraud-polygon dispatch in
the software GPU. The backend now draws inclusive flat-line endpoints and
untextured Gouraud triangles/quads, retaining clipping, masks and blending.
Quads use the documented two-triangle split and existing shared-edge ownership.
These command and endpoint rules follow the
[GPU command specification](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/).
Dithering, diagonal-line tie behavior and exact hardware interpolation remain
incomplete; the axis-aligned window fixture does not establish all of them.

Regression evidence: the original window fixture fails on its missing upper
edge before the fix, passes afterward, and fails again when line dispatch is
removed. Keeping lines but removing Gouraud dispatch instead fails its gradient
pixel. Independent GP0 fixtures cover inclusive/coincident line endpoints and
interior Gouraud rows, and the window/text fixture exercises `text_format_number`,
`text_write_season`, `text_write_weekday`, `text_list_append`,
`text_match_either`, `text_window_draw` typing and close states with `DR_AREA`
clipping, `menu_box_animate`, `heart_icon_draw`, `name_entry_start`/
`name_entry_step`, `item_villager_reaction`, `farm_clamp_status`,
`farm_adjust_stamina_fatigue`, `item_use_coin`, `picture_grid_clear`,
`picture_slot_claim`, `picture_release_item`, `item_picture_slots_clear`,
`item_picture_slots_unmark`, `title_step_dispatch` new-game initialization and
continue-load transitions, and `staff_tick` entry and route-dependent exit
transitions. The original window drawing code is unchanged.

## Controller math compatibility

`sdk_math` supplies `csqrt`, `ratan2`, `rsin` and `rcos` used by the original
analog reader and polar/camera geometry.
`csqrt` preserves the fixed-point CORDIC shifts, including the repeated stage,
rather than substituting a floating-point square root. `ratan2` preserves SDK
normalization and quantized integer angles, and `rsin`/`rcos` match all 1,025
first-quadrant 1.12 fixed-point sine entries across all four quadrants. Its
generated include contains numerical function results, not SDK instructions,
headers or binary data files.

The optional `tools/check-sdk-math.py` checker executes only these SDK math
microcalls from the locally built reference ELF in Unicorn. The CPU model
uses an explicit hook for COP2 leading-sign-bit count; it does not replace
atan/CORDIC computation. It compares every 0..128 pair used by the analog
reader with the native executable (16,641 pairs), including square-root input
`(x*x + y*y) << 12`. This is a synthetic SDK check, not a gameplay trace.
It does not establish all 32-bit math inputs or GTE hardware fidelity.

Normal builds and CTest use fixed numerical fixtures and need no SDK or VM.
To reproduce the additional check after building the reference and native
profiles, install the optional checker dependency `unicorn==2.1.4` into your
development Python environment and run:

```powershell
python tools/check-sdk-math.py
python tools/check-sdk-math.py --native build/native/Release/sdk_math_test.exe
```

`--write-table` regenerates the numerical angle results before compilation.
Keep reference ELF/map files and any SDK material under ignored build storage.

## Original controller polling

`game_pad` compiles `src/main/input/pad.c` and
`src/main/input/pad_vibration.c` unchanged. `sdk_pad` supplies its
SDK receive buffers and actuator transport; the host submits raw button bits
and 0..255 stick axes. Dead zones, headings, tilt, repeat channels, d-pad
priority, replay swapping and vibration scripts remain original game code.
The native cue-header compatibility wrappers only change variadic macro
spelling; their implementation bodies remain shared and PS1 hashes match.

The SDK bridge supports direct ports 0 and 0x10. A connected device is ready
at the host boundary; hot swaps report discovery so original code resets its
mode. Analog actuator alignment observes a three-vblank busy period before
requests are consumed. This models the used SDK protocol, not full serial or
Multi Tap emulation. `ChangeClearPAD(0)` has no native interrupt to alter;
unsupported interrupt-clearing modes stop instead of pretending to work.

The SDL host merges keyboard/controller raw bits into port 1, assigns a second
physical SDL gamepad to port 2 when connected, and supplies rumble output for both
opened gamepads. The smoke harness still uses its old input record while original
startup is pending; raw SDK samples are ready for the original poller. Rumble,
disconnects and controller feel still require developer validation.

Fixtures supply the actual original main/farm record types, not replacement
controller algorithms. They cover both ports, repeated and newly pressed
buttons, analog priority, an exact diagonal sector boundary, replay while
disconnected, vibration script step durations/deduplication/replay suppression
through actuator requests, and analog/digital/analog hot swaps.

## Debug menu and minigame overlays

`game_window_frame` compiles `src/overlay/08_DEBUG/debug.c` alongside minigame
overlays `src/overlay/17_GF_MG1/mg1.c`, `src/overlay/19_GF_MG2/mg2.c`,
`src/overlay/21_GF_MG3/mg3.c`, `src/overlay/23_GF_MG4/mg4.c`, and
`src/overlay/25_GF_MG5/mg5.c`.

`08_DEBUG/debug.c` drives the original scene selector and vibration diagnostic
screens via `KanjiFntOpen`, `KanjiFntClose`, `KanjiFntPrint` and `KanjiFntFlush`.
In `Debug` builds these route to formatted console streams (`stderr`); in
`Release` (`NDEBUG`), font diagnostics compile out entirely.

Minigame overlays 1 through 5 preserve their original simulation logic, race
tuning, AI actor thoughts, collision checks, timer formatting, and scene
request handoffs (`SCENE_FARM_MINIGAME_EXIT`). GTE rotation/translation
helpers (`RotMatrix`, `SetRotMatrix`, `SetTransMatrix`, `RotTrans`, `csin`,
`ccos`) and SPU channel bitmask macros (`SPU_00CH`..`SPU_23CH`) are implemented
in `sdk_math` and `libspu.h`. A nested helper in `mg2.c` (`mg2_draw_board_marker`)
is hoisted to file-scope `static` under `VANILLABTN_NATIVE`, leaving the PS1
nested function syntax intact when compiling the matching reference build.

## Memory-card SDK bridge and test screen

`game_window_frame` compiles `src/overlay/31_gf_mcard/mcard.c` directly without
modifying its source.

`native/compat/libmcrd.h`, `native/compat/libmcx.h` and `native/compat/kernel.h`
provide standard PS1 memory card definitions (`MemCardInit`, `MemCardAccept`,
`MemCardSync`, `MemCardFormat`, `MemCardUnformat`, `MemCardGetDirentry`,
`MemCardReadFile`, `MemCardWriteFile`, `MemCardCreateFile`, `MemCardDeleteFile`,
`McxCardType`, `McxSync`, and `struct DIRENTRY`).

`sdk_card` connects these calls directly to the host-side virtual memory card
system (`ps1_card`). Channels 0 and 0x10 map to virtual slots 0 and 1. Card probe
requests distinguish between unformatted cards, damaged/invalid headers, standard
PS1 cards, and missing cards, reporting `McxErrInvalid` for standard PS1 memory
cards (indicating non-PDA cards).

Card save file renames (`CARD_RENAME` in `mcard.c`) route through `sdk_card_rename`
via `native/compat/libapi.h`. This updates the card's directory frame filename and
recomputes the Frame XOR checksum.

GPU font helpers `FntLoad`, `FntOpen`, `FntFlush`, and `SetDumpFnt` are provided
by `libgpu.h` and `sdk_debug.c`, routing formatted output to `stderr` in `Debug`
and compiling out under `NDEBUG` in `Release`.

Synthetic fixtures in `original_window_test` exercise the full lifecycle of
`mcard.c`: initial menu rendering, card probe resulting in `MCARD_CARD`, save file
rename from development name `BISLPS-00000-FARM0` to release name `BISLPS-02489-FARM0`,
directory listing and block counting, unformatting ('MC' magic invalidation),
reformatting, and reboot request to `SCENE_DEBUG`.

## PocketStation Mcx SDK bridge and PDA sample overlay

`game_window_frame` compiles `src/overlay/10_PdaSamp/pdasamp.c`, with
`pda_sample_done = 0` initialized under `VANILLABTN_NATIVE` so the interactive
utility loop can be entered repeatedly.

`native/compat/libmcx.h` and `sdk_card` expose the PocketStation / Mcx boundary:
`McxAllInfo`, `McxGetApl`, `McxGetTime`, `McxHideTrans`, `McxGetSerial`, `McxGetMem`,
`McxExecFlag`, `McxExecApl`, `McxCurrCtrl`, `McxFlashAcs`, `McxSetMem`, `McxShowTrans`,
`McxSetLED`, `McxReadDev`, `McxWriteDev`, and `McxSync`.

There is no emulated PocketStation device. Commands complete with the SDK's
`McxErrInvalid` (2) for a present standard PS1 card and `McxErrNoCard` (1) for
a missing card. Unsupported commands do not write caller buffers or alter
card memory. Command IDs follow the SDK (for example serial 10, execution
flag 11, all-info 12, card-type 19). A request return value of 1 means accepted;
the operation result comes from `McxSync`, not that immediate return value.

The earlier bridge returned invented serial/time/memory and successful writes
and execution. The continuation audit removed those false responses. The
`sdk_absent_pocketstation` fixture checks numeric SDK contracts independently
of facade macros, unchanged buffers/card data and missing/invalid-port errors.

Synthetic fixtures in `original_window_test` execute `pda_sample_run()`, feeding
a simulated `PAD_SELECT` press through the native SDK pad buffers to verify loop
execution, menu input handling, and exit transition back to `SCENE_DEBUG`.

## Continuation audit: bounded SEQ opening

The native original sound loader now retains each copied sequence's archive
byte count. Immediately before `SsSeqOpen`, it supplies that exact borrowed
span through `sdk_sound_sequence_source`. This is a one-shot view: unknown
buffers and calls without a fresh view fail without reading data. The bytes
must remain live until the sequence is closed, as before.

The previous bridge scanned up to an assumed 40 KB looking for raw `FF 2F`
bytes. That could read past a buffer and mistake delta-time bytes for an end
event. It also published a handle when `ps1_seq_open` rejected the header.
Opening now uses the bounded parser and publishes only a successful result.
Fixtures cover invalid PPQN, a short header, an unregistered buffer and a
valid `FF 2F` delta followed by the actual end event. The invalid-header check
fails on the previous implementation and again on temporary reversion.

## Farm source compilation

`game_farm` includes the remaining original farm translation units and the
`30_GF_swind` utility. MSVC compatibility changes expand GNU case ranges into
the same integer labels, hoist capture-free nested helpers under native guards,
and use byte-pointer arithmetic where GNU C accepted `void*` arithmetic.
The PS1 branches retain their nested helpers/ranges and still match all 14
reference code hashes. Existing interrupted shop/helper/utility edits are kept.

The farm target rejects implicit function declarations (`/we4013`). Remaining
GS light descriptors and matrix-entry declarations match the installed SDK,
but declarations alone do not implement that linkage. The archive compiles in
Debug/Release and the existing 27 synthetic checks pass. It is not linked into
the host executable and is not a playable farm: unresolved host/runtime entry
points and serialized/host pointer boundaries still need work. In particular,
the source build still reports pointer truncations in animal actions, event
scripts/cue history and player code; those must be resolved before execution.

### Bounded memory-card file transfers

The SDK ReadFile/WriteFile bridge validates signed ranges, card presence and
all directory-chain blocks before copying. Invalid, repeated or truncated chains
leave caller buffers and saves unchanged. This follows the current internal
1-based block representation; external card-format compatibility remains under
audit. Open/ReadData/WriteData explicitly reject requests until implemented. File
request registration and completion polling follow the SDK contract; full
original save serialization remains under audit.

### Cooperative VSync callbacks

Waiting VSync calls now service the registered original callback at each
intervening nominal 60 Hz boundary. A host pump runs afterward for SDL events
and presentation. Query modes remain side-effect-free; late work delivers one
current callback instead of replaying stale draws. This is cooperative dispatch,
not an asynchronous hardware interrupt. It does not alone connect game startup.

### Original startup integration

The SDL host now mounts an imported disc tree, binds main RAM/VRAM, raw input,
cards and audio output, then calls the original main function. Its outer scene
dispatcher and inner frame loop remain authoritative. Native-only guards map
fixed frame addresses into main RAM and permit window-close cancellation.
The original GS allocator is linked into the runtime; the fixture allocator
remains available to isolated tests without duplicate symbols.

SDK bootstrap supports the current NTSC, non-interlaced graph setup, 2 MB RAM,
GS work cursor, parallel lighting and the missing translation/ZXY matrix calls.
Unsupported initialization/light modes fail explicitly. Numerical SDK rounding,
remaining farm pointer conversions, complete XA streaming, and original save
serialization remain open. This is not full-game acceptance.

The optional original_boot_test executable uses dummy SDL devices and a fake
clock against locally imported assets. Both profiles reach the original title
with 848 colored frames and 658 nonzero PCM intervals over 901 vblank pumps.
Only this test executable includes probe diagnostics. It uses in-memory cards
and performs no save-file access. The ordinary suite remains asset-free.

### Farm runtime pointer storage

Native event-block and VRAM-stash tail addresses are host pointers; on-disc
entry offsets remain 32-bit. Animal/toy buffers use byte-pointer addition and
cue histories subtract addresses before narrowing the relative offset. All PS1
expressions remain in the reference build. A synthetic original-function test
reads chunks split between VRAM and a host tail, including a wholly borrowed
tail. The previous 32-bit tail storage causes that test to fault. Test-only GS
allocation is a separate library so event integration can link original main
without pulling a duplicate allocator.

### Memory-card request completion

Exist, Accept, ReadFile and WriteFile register one operation at a time. Native
file copies finish immediately, but Sync reports one completed operation and
then idle; it no longer reports busy forever. File requests return registration
success separately from the card/file result consumed through Sync. Transfers
must have nonnegative ranges and byte counts divisible by 128. Both Sync modes
consume completion; idle and invalid modes leave output pointers untouched.
These contracts follow the linked SDK reference. An optional local reference
microcall confirmed the idle result and untouched outputs for both modes.

Directory results never exceed the supplied record capacity. Unsupported
Open/ReadData/WriteData calls reject work instead of claiming successful I/O.
The card tests cover command IDs, registration conflicts, completion consumption,
failed transfers preserving buffers/saves, and directory-buffer sentinels.

### ZXY rotation quantization

RotMatrixZXY rounds each 1.12 product before combining terms; generic matrix
composition differs by one unit for some rotations. Three local reference SDK
microcalls, including mixed positive/negative angles, supply independent
numerical fixtures. Translation fields remain unchanged.

### CORDIC trigonometric quantization

`csin`, `ccos` and `csincos` in `LIBGTE` (`COR_01.OBJ`, `COR_02.OBJ`, `COR_03.OBJ`)
implement fixed-point trigonometric functions using a 6-stage integer CORDIC
iteration followed by first-order residual scaling. The native SDK bridge replaces
previous ideal `rsin`/`rcos` table aliases with this exact quantization, matching
the reference SDK behavior across positive and negative inputs in all four quadrants.
Synthetic checks verify `csincos`, `csin`, and `ccos` boundary outputs.

### Estate queue layout

The original estate queue reset writes kind bytes at fixed PS1 FarmState offsets.
Those offsets overlap native entryStep after runtime pointers grow. Native builds
now address the same queue members directly, preserving message/timer fields;
the reference branch keeps its original byte writes. An independent guard test
starts entryStep at 1 and reproduces the old corruption to 0x00FF0001.
The extended demo probe advanced beyond this fault to initial scene selection;
full farm execution is still under investigation.

### Physical-zero data access

Original scene setup and ground consumers dereference zero addresses that map
to retail PS1 main RAM. Native code must translate these data accesses rather
than dereference Windows null or change the game's null-sentinel checks.
sdk_memory_field selects host member offsets for runtime objects and explicit
PS1 offsets for serialized RAM addresses, with range/overflow validation.
Absent marked-scene flags retain the PS1 0x28 offset even though native Model
pointers move their host field. Ground flags are translated only at unchecked
reads; callers that test for no hit still receive the original null sentinel.

Synthetic fixtures seed a nonzero byte at physical zero, read it through the
original ground helper, and exercise missing marked-entry writes. The previous
code faults on each fixture. The long headless probe now advances beyond scene
setup into farm frames; callback ABI adaptation remains unfinished.

### Native callback arguments

Two original call sites retain their receiver only implicitly through MIPS
argument registers: the harvest-helper tick callback and the player's confirm
action. Windows builds pass that same receiver explicitly. Handler selection,
cue tables and game rules remain unchanged; PS1 calls keep their original form.
An off-screen helper fixture checks cue start and timer advancement through the
actual public update function. The old native call faults; both profiles pass
with explicit arguments. The extended demo now reaches farm execution and
stops later on GPU packet validation.

### GTE depth registers

Native SZ results saturate to unsigned 16-bit values, including far-Z flags,
and RTPT retains the previous SZ3 after its three FIFO pushes. AVSZ3/AVSZ4 use
signed ZSF scales and saturated OTZ instead of plain arithmetic averages. The
SDK InitGeom scales are 341 and 256; synthetic tests cover fractional results,
far Z, negative scales and OTZ saturation. See the
[GTE register specification](https://psx-spx.consoledev.net/ps1/cpu/gte/geometrytransformationenginegte/).
Full MAC/IR and perspective-division precision remain open.

The extended Debug probe reaches 5,001 pumps in the original farm without a GPU
link fault. Colored output includes sprites/UI and does not prove 3D terrain:
the developer reported black farm geometry, and NCLIP winding needs correction.

### NCLIP terrain culling

NCLIP now uses the hardware signed shoelace area and reports MAC0 overflow.
The previous sign was reversed: sprites/UI remained visible while original
terrain faces were discarded. An independent screen-space area fixture verifies
backface rejection and, after reversing the TMD indices, a visible green triangle
through original model.c. Scalar fixtures check both winding signs and 32-bit
overflow. The old implementation fails both winding tests.

A local headless frame captured after the fix shows farm terrain instead of the
black area reported in the developer's window. This confirms the culling fix,
not full gameplay, lighting or audiovisual fidelity.
