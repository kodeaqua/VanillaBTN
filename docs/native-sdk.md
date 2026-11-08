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
They supply the main module's frame pointer as fixture state. Uniform texels
isolate packet generation from the backend's incomplete polygon interpolation;
independent nonuniform hardware fixtures remain required for texel fidelity.

`game_frame_buffers` compiles the original frame layout, cursor insertion,
`src/main/geometry/math_util.c`, `src/main/graphics/clut.c` and
`src/main/ui/loading_screen.c`. Native insertion replaces only the 24-bit
pointer cast with SDK link mapping. `SetDrawLoad` constructs inline `DR_LOAD`
(`GP0 0xA0`) CPU-to-VRAM transfer packets while preserving any link already
stored in the tag's low 24 bits by `ot_insert_prim`, and `ps1_gpu_exec` copies
those halfwords into VRAM with bounds and word-count validation. The original
OT dimensions, memory offsets, clearing, cursor advancement, CLUT palette
blending/bouncing/wrapping, loading-picture texture upload, double-buffered
loading OT/packet slots and environment flips remain authoritative. Tests
allocate a sufficiently large host region, map it once, invoke the original
layout for both frames, submit inserted packets, and exercise loading-screen
and CLUT-fade/scroll execution. Frame storage is runtime-only; its host
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

## Original archive reads and CD audio control

`game_afile` compiles `src/main/cd/afile.c`, `src/main/sound/cd_audio.c` and
`src/main/game/event_flags.c` against `sdk_cd`, `sdk_sound` and `sdk_sync`.
The native-only initialization scratch is one sector instead of borrowing the
host farm-state object. It checks the 33-word HDT length, aligned monotonic
offsets and terminal BIN size before the original member table is used. PS1
initialization and its reference build retain their original declarations.

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
`cd_audio_fade_tick` and `cd_audio_stop`. Starting an archive sector read via
`afile_read_bytes` invokes the real `cd_audio_stop` and switches the drive back
to data mode (`0x80`). Feeding `cd_audio.c` filter/location/volume state into
the live `ps1_xa` audio mixer stream remains a separate integration step.
File lookup failure is real failure: the host must validate the data tree before
entering original retry loops. Mount/unmount invalidates all pending requests.

The archive and CD audio fixture exercises `xa_search_files`,
`cd_audio_play_track` (valid, out-of-range and event-flag muted tracks),
`cd_audio_fade_tick` (fade-out to stop, end-of-track stop and looped track
rewinds), `cd_audio_stop`, `event_flag_set`/`event_flag_get` bitfields, and
`afile_read_bytes` stopping active XA playback. It also tests BCD conversion,
missing files, traversal rejection, read cancellation, padding, extent bounds
and an oversized HDT rejected before it can overflow native scratch.

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

`game_screen` compiles the original `screen_fade.c` unchanged. The fixture
provides main-loop frame globals and records/isolates main control boundaries;
fade counters, private packets, blend mode and shader levels come from the
original code. Synthetic in/out frames verify resulting VRAM values. This
does not wire startup or replace its publisher/title sequence.

`game_window_frame` compiles the original bevel/gradient drawing routine.
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
interior Gouraud rows. The original window drawing code is unchanged.

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

The SDL host merges keyboard/controller raw bits into port 1 and supplies
rumble output for its opened gamepad. The smoke harness still uses its old
input record while original startup is pending; raw SDK samples are ready
for the original poller. Port 2 is exercised synthetically but has no second
physical SDL controller assignment yet. Rumble, disconnects and controller
feel still require developer validation.

Fixtures supply the actual original main/farm record types, not replacement
controller algorithms. They cover both ports, repeated and newly pressed
buttons, analog priority, an exact diagonal sector boundary, replay while
disconnected, vibration script step durations/deduplication/replay suppression
through actuator requests, and analog/digital/analog hot swaps.
