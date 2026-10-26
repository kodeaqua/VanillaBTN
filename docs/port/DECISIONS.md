# Architecture decisions

## ADR-001: Isolate the Phase 0 build boundary

Date: 2026-10-25. Status: accepted, following AGENTS.md defaults.

Context: The matching project installs a PSX toolchain before project creation.
The native port must work independently and must not require proprietary SDK input.

Decision: Keep the native CMake project and its presets in `port/`. Begin with
a C11 fixed-width type/endian test. Preserve all matching source and link rules.
Prepare matching dependencies using verified archives on Linux, with a local
owner-supplied SDK. Port checks run without network or game assets.

Consequences: Run port presets from `port/`. A foundation build does not imply
game boot. Renderer selection, overlay namespacing, asset layout changes, Android
minimum SDK and native host-tool version pins remain unresolved. No enhancement
is enabled. Workflow/CI work is deferred by owner instruction.

## ADR-002: Bring up Windows first

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The original roadmap starts native bring-up on Linux. The owner asked
to prioritize Windows during Phase 0.

Decision: Bring up Windows x86-64 first, using project-local checksum-pinned
LLVM-mingw/UCRT, CMake and Ninja. Windows port builds must not depend on WSL,
a PSX SDK, or the matching toolchain. Retain the other desktop foundation
presets for later validation. Matching validation retains its Linux requirement.

Consequences: The first game boot target is now Windows. Android is deferred.
No renderer or runtime architecture decision changes. Release links use static
compiler runtime dependencies and the Windows system UCRT; system DLL imports
remain expected.

## ADR-003: Validate locally and defer CI

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner explicitly excluded workflows and CI from the current work.

Decision: Leave existing workflow files untouched; add no CI provisioning or
jobs. Validate the Windows foundation locally and document actual results.

Consequences: Roadmap and completion requirements for a CI skeleton are deferred.
Existing upstream workflow behavior is not validated or endorsed by this work.

## ADR-004: Exclude WSL from Windows port work

Date: 2026-10-25. Status: accepted by explicit owner instruction.

Context: The owner does not want WSL used for this work.

Decision: Build and validate the port with native Windows tools. Do not install,
invoke, or require WSL. Defer matching validation to a separate Linux host when
available, preserving the requirement to report an unverified matching baseline.

Consequences: An unavailable Linux matching toolchain does not block Windows
port progress. Matching source protection and honest validation reports remain
required. No claim of a successful matching check follows from Windows tests.

## ADR-005: Upgrade the Windows compiler for sanitizer startup

Date: 2026-10-25. Status: accepted within the existing LLVM-mingw toolchain choice.

Context: LLVM-mingw 20241001 / Clang 19.1.1 compiled the sanitizer targets but
ASan crashed before main with `interception_win: unhandled instruction`
(`44 0f b6 1a`). This matches the upstream Windows interceptor issue:
https://github.com/mstorsjo/llvm-mingw/issues/477.

Decision: Pin LLVM-mingw 20250709 / Clang 20.1.8 and its archive SHA-256.
Keep ASan and UBSan enabled together in `windows-asan-ubsan`; never suppress
the startup issue by disabling interceptors. Sanitizer builds use the toolchain's
ASan DLL on PATH; release builds retain static compiler runtime linkage.

Consequences: Setup downloads a new compiler archive once. The build wrapper
uses `cmake --fresh` to preserve preset options across compiler upgrades; CMake's
automatic compiler-change reconfiguration can otherwise discard options such
as HMBTN_SANITIZERS. Old ignored tool directories need not be deleted.

## ADR-006: Keep SDK words fixed-width and host descriptors native

Date: 2026-10-25. Status: accepted, following the existing memory/type defaults.

Context: Windows host sys/types.h does not provide the SDK's u_long spelling.
Using host long for it would also become 64-bit on LP64 platforms. GPU packet
words must remain 32-bit while host pointers must retain their full width.

Decision: Force-include `compat/game.h` only in game-facing translation units.
It loads host sys/types.h first, then maps u_char/u_short/u_long to namespaced
fixed-width aliases. Do not redefine the C keyword long or force this mapping
into the platform/runtime targets. GPU packets use 32-bit tag words; GS ordering
table descriptors use native pointers. Assert packet and pointer-free asset
layouts without asserting the PS1 size of pointer-bearing host state.

Consequences: This establishes declarations and layout checks, not a GPU runtime
or a solution to truncated links in original LINK_PRIM. Original data loaders,
overlay dispatch and save serialization still need a separate 64-bit audit.
Public packet format reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

## ADR-007: Compile the original entry point before linking or executing it

Date: 2026-10-25. Status: accepted, following the separate native-build default.

Context: SDK declarations blocked main.c before native code generation. Even
after declaration fixes, its hardcoded work addresses and 32-bit pointer casts
make it unsafe to run on a native 64-bit host.

Decision: Compile unmodified main.c into the native game archive with a scoped
`main=hm_game_main` definition. Add its source-required SDK declarations and
compile afile.c in the same archive. Do not link or execute a game entry point
until native work memory, overlay dispatch and required runtime calls are ready.
Implement only the pure CD position helpers now; disc IO calls used by afile.c
abort explicitly. Other declared-but-unimplemented APIs remain link errors.

Consequences: Native code generation is verifiable without falsely claiming
boot. CdlFILE is source-facing search metadata containing only pos and size,
not a copied SDK ABI or an on-disc record. CdPosToInt reads exactly three bytes
because cd_audio.c passes the last three bytes of a GetlocP response. CdIntToPos
preserves the reserved fourth byte. Tested inputs cover normal two-digit BCD
positions, not malformed locations or arbitrary out-of-range LBAs.

Hardware reference for BCD MSF, 75 frames/second and the lead-in offset:
https://psx-spx.consoledev.net/cdromdrive/.

<a id="adr-008"></a>

## ADR-008: Back the asset/frame workspace with normal C storage

Date: 2026-10-25. Status: accepted, following the native memory default.

Context: main.c treats 0x801BE1B0 as a writable frame workspace. Its frame buffers
occupy 0x3EE50 bytes; the input demo and dynamically loaded farm data sit below
it. Replacing only the frame address would leave pointer subtraction outside
the allocated object. farm_state_init also derives its load window by subtracting
overlay code addresses, which does not apply to statically linked host overlays.

Decision: Use one aligned C byte array solely for this existing contiguous data
workspace, not a 2 MB PS1 memory image. Keep the 0x49000 image window, 0x47390
section/free capacity and 0x3EE50 frame work area. The capacity is derived from
the matching addresses: align the low-to-high region distance (0x75FC8) to
0x800 bytes, producing load window 0x8012DE20 and section start 0x80176E20;
the distance to WORK_REGION is then 0x47390. Native frame/tag offsets remain
unchanged. Overlay code and other globals/heap remain outside this storage.

Native source hooks will select the buffer and return a byte pointer from
main_get_region_base, preserving full host pointer width. Remaining overlay
callers that assign that pointer to 32-bit integers must fail compilation until
ported. The matching branches retain their original declarations and statements.
No save-format or asset-pointer compatibility is implied by this workspace.

Consequences: Frame/farm memory can be tested independently of GPU execution
and disc IO. Free-space budgets and demo overlap retain their vanilla meaning.
OT pointer encoding, overlay loading, asset bounds validation and the rest of
boot remain separate work. Matching check still requires an unavailable Linux
toolchain; textual branch preservation is additional evidence, not a substitute.

<a id="adr-009"></a>

## ADR-009: Encode GPU links as stable handles to native tag pointers

Date: 2026-10-25. Status: accepted, following the fixed-width handle default.

Context: PS1 packet tags have an eight-bit command length and a 24-bit next
address. Truncating a native pointer can alias unrelated objects and cannot
represent 64-bit host addresses. The source links packets through SDK addPrim,
the LINK_PRIM macro, and ot_insert_prim; all three paths must agree.

Decision: Preserve packet layouts and replace only the address part with a
nonzero 24-bit handle. Keep 0xFFFFFF as the end marker. Assign handles in first-use
order, storing full native pointers in a bounded registry. Hash buckets are an
internal lookup detail; ASLR does not affect handle assignment. The initial
registry supports 65536 live tag addresses with a fixed 1 MiB allocation (pointer
array plus buckets) and no per-frame allocation. Exhaustion and malformed handles
abort with diagnostics rather than truncating, wrapping or silently succeeding.

ClearOTagR creates reverse tag chains. addPrim/AddPrim preserve both packet and
head lengths; the source-specific helpers retain LINK_PRIM/ot_insert_prim's
behavior of clearing the OT head length. setlen writes only the length byte,
without reading uninitialized address bytes. DrawOTag/rendering remain separate.

Consequences: Callers must keep registered tag objects alive and discard every
chain before resetting the registry. Do not reset it per frame; copied/cached
packet links must remain valid. Handles are process-local, not save-file data.
The registry validates identity/alignment, not allocation extents or payload
lengths; it is not an untrusted-memory sandbox or a cycle detector. Capacity and
runtime cost still need full-game/device profiling; no performance claim is made.
Matching source integrations must retain the exact original else branches.

Public packet/OT reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-010"></a>

## ADR-010: Keep native stacks and explicit diagnostic markers

Date: 2026-10-25. Status: accepted, following the native ABI/scratchpad default.

Context: cpu_asm.h switches to a MIPS scratchpad stack and stores return addresses
in 32-bit diagnostic words. Neither operation is valid on a 64-bit host. Source
inspection shows no game-data accesses to the saved scratchpad SP; lastCaller is
a diagnostic word also copied into save metadata, not an executable function pointer.

Decision: Native calls retain their normal ABI stack. Provide aligned 1 KB C
scratchpad storage and no-op only the stack-switch hints. SAVE_RETURN_ADDRESS
writes the deterministic 0x504F5254 PORT diagnostic marker instead of truncating
a host address. A game debugger-wait request reports a runtime error and aborts,
rather than emitting a MIPS break instruction or hanging the native process.
Keep all original CPU macros in an exact matching else branch when integrated.

The SDK csin/ccos/csqrt names are mapped to explicit fixed-math declarations,
avoiding collisions with the host C99 complex builtins. This does not yet
implement their arithmetic. Packet helpers implement only source-used fields
and GP0 encodings; video dimensions 320x240 follow the original main.c setup.

Consequences: These hooks unblock native compilation, not GTE functionality or
save-format parity. No approximation of an unimplemented GTE operation is added.

<a id="adr-011"></a>

## ADR-011: Mount validated raw tracks through platform-owned streams

Date: 2026-10-25. Status: accepted, following the original-disc input default.

Context: The game's archive reader needs ISO metadata and 2048-byte sector reads.
Desktop paths and Android descriptors must not enter the portable CD parser.

Decision: The platform owns an exact positional-read callback and stream length.
Hash the complete raw BIN against disc/disc.sha1 before mounting ISO-9660.
Decode sector headers explicitly; accept Mode 1 and Mode 2 Form 1 for filesystem
reads and report unsupported Form 2 transfers. Validate directory record lengths,
both-endian fields and extents without casting disc bytes to host structs.
Keep all game data external. A local desktop probe provides validation before
the SDL first-run UI exists; it is not a runnable port of the game.

SDK reads use a single game-thread controller with an explicit 60 Hz tick and
75/150-sector-per-second transfer budget, retaining fractional sectors. Polling
does not advance time. Blocking ReadSync drains the request without host sleeps.
No seek delay is added during bring-up. The eventual VSync platform integration
must call this tick; no host callback thread may advance game state. Unsupported
commands (including XA streaming) fail explicitly, not with simulated success.

Consequences: Normal data reads and errors can be tested without disc assets.
Exact PS1 seek/retry/status timing, CUE input, XA streaming and full boot remain
unverified. SHA-1 is a known-image identifier, not an authentication mechanism.
The implementation is original port code, not copied from the RFC example.

References: https://psx-spx.consoledev.net/cdromformat/,
https://psx-spx.consoledev.net/cdromdrive/,
https://www.rfc-editor.org/rfc/rfc3174.

<a id="adr-012"></a>

## ADR-012: Keep a native VRAM transfer model before renderer submission

Date: 2026-10-25. Status: accepted, following the 1024x512 VRAM model default.

Decision: Keep 16-bit VRAM in ordinary C storage. LoadImage/StoreImage transfer
little-endian pixels with independent X/Y wrapping and hardware copy-size masks.
Transfers execute synchronously; DrawSync observes no pending work. ClearImage
uses GP0 fill rounding and bypasses mask bits. Track changed 16x16-word tiles for
future renderer uploads, with explicit acknowledgement rather than per-frame
allocation. Preserve STP bits and implement set/check write masks.

Consequences: Original texture-pack code is now compiled and tested on synthetic
raw and RLE banks. This is not the SDL_GPU backend or a completed command
rasterizer. MoveImage, OT command execution, displayed frames and full rendering
parity remain pending. StoreImage leaves unused odd-pixel transfer padding alone;
the game's callers consume pixels, not padding. No claim is made about bus
padding parity or GPU timing. No speculative rendering enhancement is added.

Reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-013"></a>

## ADR-013: Hoist capture-free nested helpers through native adapters

Date: 2026-10-25. Status: accepted, following the narrow source-guard default.

Context: The original GCC code defines functions inside other functions. Clang
rejects these definitions. Several helpers use only arguments and file globals,
so moving their scope does not require closures or game-state changes.

Decision: Keep original nested definitions untouched in matching else branches.
For native builds, include static copies from port/compat/nested immediately
before the enclosing function. Hoist only helpers inspected to capture no local
variables. Keep names, parameter types, expressions and branches equivalent.
Do not apply this pattern to captured-local helpers without explicit arguments.
A regression test compares original and adapter tokens, excluding only static
and whitespace/comments. Existing unusual comparisons remain unchanged.

Consequences: Eleven capture-free helpers in ground collision, animal headings,
estate items, shop rows, winter event conditions, scene visibility, minigame 2
and debug screen storage now compile under Clang.
Data-free tests exercise winding/edge/height geometry, comparison boundaries,
estate flag priority and visible medal rows. These do not prove full gameplay
parity. The native helper copies must remain synchronized with matching source.
The matching compiler check remains unavailable on the Windows-only host;
reconstructing exact matching text provides additional evidence, not a binary
matching result. No compiler switch or new runtime dependency is introduced.

The harvest carry helper captures its enclosing object. Its native adapter
receives that object explicitly; a call macro passes the current object and is
undefined at the end of the enclosing function. The function body is unchanged.
A token test checks its body, and a C test passes different captured/argument
objects to verify the distinction is preserved.

<a id="adr-014"></a>

## ADR-014: Dispatch source GTE commands into native scalar state

Date: 2026-10-25. Status: accepted, following the clean-room GTE default.

Decision: Replace the two raw MIPS command macros only in the native branch.
The source-used inline load/store helpers operate on native C GTE state, with
fixed-width packet stores. RTPS/RTPT use the public UNR reciprocal calculation,
44-bit wrapping, signed shifts, FIFO movement and documented saturation/flags.
Use integer arithmetic throughout, including exceptional quotient saturation
without divide-overflow. Matrix stack snapshots preserve rotation/translation;
source-used projection setters update the same state.

Consequences: The original gte_project_vertex is exercised by a fixture with
traps for unrelated camera/draw dependencies. Normal XY/depth and behind-camera
rejection pass. GTE vectors also cover triple FIFOs, packet field placement,
clipping, flags and in-place matrix multiplication. Color commands, uncommon
MVMVA selectors, high-level trigonometry/normalization and complete SDK init
defaults are not parity validated. Unsupported calls still fail explicitly.
InitGeom supplies the source's OT scaling; full hardware captures and replay
comparisons remain required. No CPU interpreter or SDK implementation is copied.

Reference: https://psx-spx.consoledev.net/geometrytransformationenginegte/.

<a id="adr-015"></a>

## ADR-015: Separate native SDK metadata from asset ABIs

Date: 2026-10-25. Status: accepted, following source-facing compatibility defaults.

Decision: Declare only pad/card/PocketStation APIs observed in the source.
DIRENTRY is native result metadata with a terminated filename and fixed-width
size, not an SDK/on-card struct. McxFuncExecFlag is an opaque native completion
identifier (0x100), not a guessed SDK opcode or a PocketStation wire command.
The native backend must translate its own identifiers; unsupported PocketStation
commands currently abort. No protocol implementation is claimed by declarations.

The three text_fmt_* strings are defined static and used only inside text.c.
Suppress conflicting extern declarations in the native header branch while
retaining the original declarations in the exact matching else branch.

Consequences: All source units can be diagnosed with typed native declarations.
Windows long-pointer type warnings remain visible; LP64 ports require further
fixes before runtime validation. Card persistence and host pad polling are
unfinished. No proprietary header layout or code is imported.

<a id="adr-016"></a>

## ADR-016: Keep event staging addresses native and asset offsets fixed

Date: 2026-10-25. Status: accepted, following native pointer/asset-offset defaults.

Decision: Use uintptr_t for the event block base, VRAM-tail staging address and
their address temporaries. Preserve alignment checks on the full host address.
Read signed 32-bit entry offsets through a little-endian byte helper, without
casting a host pointer to s32 or restricting a terminal offset to the declared
array prefix. Asset bounds remain the caller's responsibility. Assert the
FarmEventBlock fixed prefix is 148 bytes, with entry offsets at byte 52.

Consequences: Runtime addresses grow without changing disc offsets or packed
data. Source changes remain narrow guarded declarations/expressions. Asset-word
tests include unaligned signed reads under sanitizers. Real event playback,
all remaining pointer/layout hazards and save ABI parity are not validated.

<a id="adr-017"></a>

## ADR-017: Link native overlays and reject unsupported reentry

Date: 2026-10-25. Status: accepted for initial bring-up; reset lifecycle pending.

Decision: Compile all 113 original C units, including all thirteen overlays,
into the native archive. Their existing source names have no duplicate strong
external definitions; preserve resident imports without blindly prefixing them.
The native LOAD_OVERLAY path selects a validated member/region registry entry
instead of copying PS1 executable bytes. Existing asset-region references bind
to the native data workspace. Compiled C initializers supply first-use state.

Consequences: The full graph links into the boot diagnostic. A second activation
of the same overlay aborts explicitly until native data/BSS restoration is
implemented; silently retaining mutated globals would change vanilla behavior.
Registry reset resets metadata only. No reentry, save/load lifecycle or full
playthrough parity is claimed. No BIOS, dynamic module or executable disc data
is loaded. The user's image remains required for original non-code assets.

<a id="adr-018"></a>

## ADR-018: Validate SDL_GPU presentation on Windows Vulkan

Date: 2026-10-25. Status: provisional for Windows; other devices pending.

Decision: Pin SDL3 3.2.28 with the publisher's GitHub asset SHA-256 digest and
retain its zlib license. Build an optional standalone presentation spike behind
HMBTN_BUILD_GPU_SPIKE=ON, leaving data-free unit builds offline by default.
Keep all SDL headers in port/platform. Upload a synthetic 320x240 RGBA texture,
blit it to an offscreen target, verify byte-exact GPU readback and present eight
frames, with at most two frames in flight. No custom shader dependency is needed
for this blit test.

Result: Passed using SDL_GPU Vulkan on the owner's Radeon RX Vega / Windows
driver 31.0.21925.1001. The GPU shader blit and swapchain were exercised. This
does not demonstrate PS1 primitive rasterization, a rendered game frame, D3D12,
Metal, Android or performance budgets. SDL_GPU remains the first backend route;
cross-platform suitability still needs validation. No fallback is selected here.

References: https://github.com/libsdl-org/SDL/releases/tag/release-3.2.28,
https://wiki.libsdl.org/SDL3/SDL_BlitGPUTexture.

<a id="adr-019"></a>

## ADR-019: Advance callbacks and silent sound state on the game thread

Date: 2026-10-25. Status: accepted for bring-up, following deterministic execution.

Decision: VSync waits advance explicit 60 Hz ticks; query modes do not advance
time. Advance CD transfers and VBlank callbacks at these points. A platform
wait/scanline hook supplies host pacing/profiling; the console probe uses virtual
time. Preserve callbacks across repeated ResetCallback calls from archive reads.
SetMem(2) validates the original BIOS hint without allocating an emulated RAM
image. SystemError reports the original error words and aborts.

Implement real silent sound initialization/configuration: sequence table limits,
reserved voices, common volume masks, stereo/CD routing and start/stop state.
Key statuses are genuinely off because bank loading and key-on still abort;
no playback is falsely marked complete. Configuration-only acknowledgements
must not be mistaken for an implemented mixer. VAB/SEQ decoding, live voice
mixing, ADSR, reverb, XA and audio output remain pending.

Native GPU environments now retain clip/offset/display state and GS work-buffer
pointers; background clears update the actual VRAM model. ResetGraph preserves
VRAM and OT handle identity. DrawOTag, MoveImage and unsupported scanout modes
still abort. The window presentation spike is separate from the game probe.

<a id="adr-020"></a>

## ADR-020: Bound native title-picture copies to their source arrays

Date: 2026-10-25. Status: accepted for native memory safety.

Observed: ASan boot with the owner's verified disc reports a 6144-byte read
from title_image_1, whose declared length is 4200 bytes. The original fixed-size
TitleImage assignment crosses into neighboring matching globals. All three
pictures are smaller than the destination. Native global placement and ASan
redzones cannot reproduce that incidental adjacency.

Decision: A narrow HMBTN_PORT branch selects the same random picture once and
copies its declared complete bytes. Leave the destination tail untouched; it
is outside that picture's compressed asset. The original assignment remains
in the matching else branch. This avoids reading unrelated native globals
without modifying the asset or suppressing ASan. No visual parity is claimed
until the loading-picture decoder and screen are exercised. Matching check is
unavailable on this Windows-only host; textual else reconstruction is separate
evidence, not a matching build result.

<a id="adr-021"></a>

## ADR-021: Implement real device absence and incremental GPU/audio assets

Date: 2026-10-25. Status: incremental bring-up; parity pending.

Digital pad replies publish sampled buttons in original wire order, including
after the game clears its reply buffer. The current memory-card controller has
no mounted card and completes detection with a real no-card error. File writes
remain unsupported; no persistence is advertised.

Generate an original nearest-integer Q12 quarter-wave table from the analytic
sine function, rather than importing SDK table data. rsin/rcos wrap at 4096
units; exhaustive tests compare the analytic reference. Proprietary SDK table
quantization parity is unverified and remains a fidelity gate.

DrawOTag walks native handles and executes environment registers, inline VRAM
uploads, fills, tiles, sprites and axis-aligned flat quads into reference VRAM.
Texture depth, CLUT, texture-window, modulation, mask, clip and blending state
are applied. Other polygons/opcodes still abort explicitly. This is an initial
CPU correctness path, not a measured final renderer or a performance-budget
claim. A separate optional SDL_GPU game-window target uploads current scanout;
the standalone synthetic hardware spike remains available.

VAB v7 headers and streamed sample bodies are owned native assets with bounded
allocation, sparse program metadata and synchronous chunk copies. This is
actual bank loading, not sample playback. Sequence/key-on/mixer/reverb work
remains unfinished. The real-disc sanitizer probe now stops at SsSeqOpen.

Reference: https://psx-spx.consoledev.net/cdromfileformats/#cdrom-file-audio-sample-sets-vab-and-vhvb-sony
and https://psx-spx.consoledev.net/graphicsprocessingunitgpu/.

<a id="adr-022"></a>

## ADR-022: Retain sequence asset bounds at the native loader boundary

Date: 2026-10-25. Status: accepted for safe SEQ parsing; playback pending.

SsSeqOpen receives only a pointer and bank ID. The original loader knows the
exact copied sequence size; attempting to discover an end marker through an
unbounded pointer would hide malformed-asset reads. A narrow native guard
registers the copied pointer/length with the sequence runtime. Matching code
does not call the registration function and reconstructs the baseline exactly.

The parser checks SEQ v1 headers, big-endian timing, bounded variable integers,
channel event widths, running status and metadata/SysEx payload bounds. Open
validates termination and requires a loaded bank. Handles and volume settings
are retained; stopping an idle opened sequence is valid. SsSeqPlay still aborts
until the sequencer and voice mixer can actually perform the requested playback.
No silent playing/completion status is fabricated. The real-disc sanitizer
probe now reaches SsSeqPlay after opening the sequence successfully.

Reference: https://psx-spx.consoledev.net/cdromfileformats/#cdrom-file-audio-sequences-seqsep-sony.

<a id="adr-023"></a>

## ADR-023: Build a deterministic native SPU voice core from hardware facts

Date: 2026-10-25. Status: reference bring-up; game audio parity unverified.

Decode SPU-ADPCM blocks with explicit signed arithmetic, predictor history and
saturation. Advance ADSR at 44.1 kHz, including slow-rate counters and frozen
rates. A 24-voice native mixer uses bounded bank assets, loop/end flags, pitch
accumulators, running interpolation history and Gaussian hardware coefficients
documented by psx-spx. The coefficient table contains hardware numeric facts;
no SDK or emulator implementation was copied. Voice mixing allocates nothing.

Native key-on/off and status APIs now operate on these real voices. Sparse VAB
programs map to packed tone blocks; sample ranges are checked against uploaded
body size. Reference-counted sample bodies survive closing a bank until its last
live/releasing voice finishes. There is no per-note allocation. Reset stops
voices before freeing banks.

Pitch uses an original analytic equal-tempered Q12 reference with 127 fine
steps per semitone, generated by port/analysis/generate_pitch.py. Exact SDK
pitch quantization, volume curves/pan laws, voice allocation/stealing, key-on
latency and full audio parity remain unverified. Do not mistake deterministic
synthetic PCM tests for proof of those properties. Sequencing, reference reverb
and host output are described below; XA remains unfinished.

References: https://psx-spx.consoledev.net/soundprocessingunitspu/ and
https://psx-spx.consoledev.net/cdromformat/#cdrom-xa-audio-adpcm-compression.

<a id="adr-024"></a>

## ADR-024: Run sequence clocks and reference reverb on the game thread

Date: 2026-10-25. Status: initial reference; audio parity unverified.

Each VBlank advances SEQ events with integer timing credits, followed by 735
stereo samples at 44.1 kHz. Program/note/volume/pan/expression/pitch events drive
actual voices. Ownership prevents later note-offs from releasing a voice
reassigned to SFX. Source-used NRPN loop markers 20/30 and data entry 6/98
support finite and endless music-part repetition. Unsupported events abort.

Configured idle sequence slots retain volume settings: the original loader
clears all six slots before testing which sequences are open. Stop/close
releases owned voices. SPU release/reverb mixing continues after SsEnd stops
sequence scheduling. Reference-counted bank bodies survive pending tails.

Room, studio-large and hall reverb use public numeric register presets, a normal
native circular delay buffer, fixed-point reflection/comb/all-pass stages and
the documented 39-tap rate-conversion filter. No SPU RAM image is allocated.
Rounding, phase/order details and complete mix/SDK volume behavior still need
golden-audio comparison. Synthetic impulse and chunk-determinism tests do not
establish bit-exact SPU behavior.

References: https://psx-spx.consoledev.net/soundprocessingunitspu/ and public
format facts in https://psx.arthus.net/sdk/Psy-Q/DOCS/Devrefs/Sound20.pdf.
No SDK code or binary data was imported.

<a id="adr-025"></a>

## ADR-025: Extend reference rasterization and connect diagnostic replay

Date: 2026-10-25. Status: Windows title bring-up; raster parity pending.

Decode flat/Gouraud textured/untextured polygons, split quads in original vertex
order, and rasterize clipped triangles with affine attributes and explicit
shared-edge ownership. Apply texture/STP, blending and dithering. Lines include
their final endpoint and apply dithering, including flat lines. This CPU path
supports bring-up and later GPU comparisons. Hardware subpixel/UV rounding,
line ties and full VRAM parity remain unverified. This is not a measured final
backend or a performance-budget claim.

Development probes accept tick/button replay files through the native pad
sampler used by keyboard input. The console probe can run bounded virtual
VBlanks; the SDL probe uses host pacing and can capture BMPs under ignored
build output. These are local validation tools, not enhancements or a packaged
replay UI. Windows Vulkan captures show the original title, New Game/Continue
menu and initial naming dialogue. Physical keyboard interaction is unverified.

References: https://psx-spx.consoledev.net/dev/ps1/gpu/render-polygon-commands/
and https://psx-spx.consoledev.net/dev/ps1/gpu/render-line-commands/.

<a id="adr-026"></a>

## ADR-026: Preserve full native text-bank addresses

Date: 2026-10-25. Status: accepted for pointer safety.

ASan New Game replay reports an access violation in name_title_build_label.
LOCALIZED casts an actual text-member pointer to u32 before adding a relocation
delta; LOCALIZED_NAME and NAME_OFFSET have the same hazard. A port-only include
uses full native addresses and the compiler's constant offsetof builtin. Keep
the bank-start fields as 32-bit relocation metadata and retain original macros
in matching else branches. Tests cover identity, relocation deltas and the
12-byte metadata layout. The replay subsequently passes name/birthday setup
and reaches farm initialization. Matching check is unavailable on Windows;
textual reconstruction is separately checked against 9ace470.

<a id="adr-027"></a>

## ADR-027: Feed SDL output through a lock-free PCM ring

Date: 2026-10-25. Status: Windows output tested; quality/latency gates open.

The game thread produces deterministic 44.1 kHz PCM into a bounded SPSC ring.
SDL's postmix callback consumes it without allocating, locking, logging or
calling game code. Lock-free 32-bit atomic indices are required at compile time.
A two-tick prebuffer absorbs ordinary jitter. The callback converts stereo to
device float channels and linearly resamples when the device rate differs.
This host conversion never changes game-thread PCM/state. Overflow drops
output frames; underflow emits silence. Counters are reported after shutdown.

The Vulkan/ASan window probe completed 1200 VBlanks and its SDL audio callback
consumed output: 919926 device frames, 65858 source underflows, zero producer
drops in that run. This validates the output path, not listening quality,
smooth pacing, latency budgets or parity. Underflows need further work.
Other platforms and device lifecycle remain untested.

Reference: SDL3 3.2.28 SDL_audio.h / SDL_SetAudioPostmixCallback contract.

<a id="adr-028"></a>

## ADR-028: Preserve initialized animation table tails

Date: 2026-10-25. Status: accepted for native metadata correctness.

The farm startup replay reports a global overread in player_init_toys. Clang
discards initializers for AnimPackToc.start[0], leaving only its count word.
The source declares 67 initialized tables using this type. The original compiler
accepted their variable tails; silently losing them corrupts resource offsets.

Use a flexible array member only for native builds. Clang's standalone flexible
member initializer extension retains each complete object while sizeof keeps
the 4-byte prefix. Read indexed table words through the byte accessor rather
than indexing beyond the scalar count member. Keep original matching fields
and macros intact. Tests check prefix/offset layout, a synthetic variable tail
and original slot/item/resource table entries. Preserve the full native asset
base when binding toy sprites. No proprietary table or disc data is added.

<a id="adr-029"></a>

## ADR-029: Clear estate queues through native fields

Date: 2026-10-26. Status: accepted for native state safety.

After retaining animation table tails, the New Game replay reports an invalid
farm entry-step index, 0x00FF0001. A temporary function-exit trace identifies
estate_queues_clear as the first function damaging that field. It writes bytes
at PS1 FarmState offsets 0x49E6/0x4A36 with a four-byte stride; native pointers
have moved those queues, so the writes hit unrelated native state.

A narrow native branch writes each queue record's kind field by name. Preserve
the original byte-only behavior: message and timer fields are untouched, and
noticesSeen is cleared afterward. Keep the original offset loop in the matching
else branch. Regression checks retain entryStep and every message/timer while
clearing both queues. The diagnostic trace stays under ignored build output;
no instrumentation or sanitizer suppression is added to production code.

<a id="adr-030"></a>

## ADR-030: Connect source-used GS projection and light setup

Date: 2026-10-26. Status: reference setup; lighting parity pending.

GsSetProjection forwards valid source distances to the native GTE projection
register. Directional light setup normalizes each opposite direction into a
light-matrix row using deterministic integer square root, and installs RGB
coefficients in the matching color-matrix column. Ambient uses Q12 inputs via
the native back-color adapter. Only the source's normal lighting mode zero is
accepted; unsupported fog modes still fail explicitly.

Axis/color/ambient tests cover the setup. Exact SDK normalization quantization
and rendered lighting parity remain unverified, like the analytic trig path.
References: public GsF_LIGHT direction/normalization and ambient descriptions
in the Net Yaroze library reference and public libgs overview; no SDK code was
copied. GTE arithmetic remains in the separate scalar command implementation.

<a id="adr-031"></a>

## ADR-031: Select the first area without reading address zero

Date: 2026-10-26. Status: native safety adaptation; hardware parity pending.

The New Game replay reaches scene_resolve_selection before any area has been
loaded. farm_state_init clears farm_area.mode.data. The two previous-mine
override checks dereference that null pointer before scene_enter loads the
first area's data. ASan/UBSan identifies the first dereference on Windows.

For native builds, apply those overrides only when previous-area data exists.
Leave the loaded-area path and original matching conditions unchanged. Do not
map address zero or simulate PS1 RAM. A test calls the original selection
function with no previous area, a house upgrade/season, and loaded mine flags.
The contents read at address zero on original hardware are not established;
this adaptation does not prove first-entry hardware parity. Matching check
remains unavailable on this Windows-only host; source-branch reconstruction
against 9ace470 passes for all 59 guards in 26 files.

<a id="adr-032"></a>

## ADR-032: Leave absent spouse-marker objects untouched

Date: 2026-10-26. Status: native safety adaptation; hardware parity pending.

After the first area's models are populated, scene_slot_flag_move searches for
two optional marked entries, then unconditionally writes both pointers. The
New Game replay reaches an area without a marked entry and UBSan reports a
null FarmSceneModel access. The source asset struct has no native pointers;
tests assert its 0xBDC size and model offset 0x948 to exclude layout drift.

Only native builds skip the flag write for an absent entry. Existing entries
receive the same flags. Keep both original writes in the matching branch.
Extend the scene regression to populate a synthetic area with every model
absent. This does not emulate the original writes to address zero or establish
their hardware effects. No mapped low-memory page or fake scene is introduced.

<a id="adr-033"></a>

## ADR-033: Supply terrain and camera vector arithmetic

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

The real-disc New Game path reaches ground_height_at and calls OuterProduct0.
Implement it through the existing scalar GTE OP command with shift zero and
preserve the rotation matrix, as documented for the non-inline SDK function.
Add source-used camera operations: ApplyMatrixLV multiplies three Q12 matrix
rows by a 32-bit vector, supports aliases and writes only three output words.
It does not add the matrix translation. This matters because the original
camera passes its three-word translation storage as the output VECTOR.

Supply integer SquareRoot0 and VectorNormal/S/SS reference implementations.
Normalize using a Q12 integer length to retain fractional lengths; preserve
output padding because the camera writes normalized vectors into matrix rows.
Return the squared input length from VectorNormal. Reject inputs outside its
documented signed squared-length range. Zero vectors produce zero components.
No host floating-point math or per-call allocation is used.

Tests cover every signed-domain square boundary, fractional/axis/zero normals,
in-place inputs, output padding, matrix/translation aliases and OP matrix
preservation. Full SDK lookup quantization, intermediate register side effects
of matrix/vector helpers and zero-vector hardware parity are unverified.
Reference: public Run-Time Library Reference 4.7, ApplyMatrixLV, VectorNormal
and OuterProduct descriptions; Inline Programming Reference OP preservation
distinction. Only API facts and the existing scalar hardware core are used.

<a id="adr-034"></a>

## ADR-034: Compose source-used Euler rotation matrices

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

After terrain cross products, the real-disc path reaches RotMatrix from the
farm view setup. Implement the documented Q12 matrix products Rx * Ry * Rz for
RotMatrix and Rz * Rx * Ry for RotMatrixZXY. Reuse the port's analytic integer
sine table, use signed floor shifts and keep translation words untouched.
Quarter-turn tests check axis signs, noncommuting rotation order, return
pointers and translation preservation. Other rotation entry points remain
explicit failures until implemented. SDK sine-table and product-rounding
parity remain open; no SDK implementation or table is copied.

Reference: public Run-Time Library Reference 4.7, RotMatrix table 8-2,
https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf (page 8-116).

<a id="adr-035"></a>

## ADR-035: Resolve player probe steps across native table objects

Date: 2026-10-26. Status: accepted for native bounds safety.

The New Game replay reaches player_probe_target and indexes halfword 56 of
move_steps_small[16]. Its callers use bases 0, 0x18 and 0x38 plus facing * 2.
These cross object boundaries in the original overlay's adjacent data tables.
A local read of the owner's validated farm overlay confirms the source order:
small (16 halfwords), large (16), riding (8), hop (8), reach (16). No data is
extracted into the repository.

Native builds explicitly select small[facing * 2], large[8 + facing * 2], or
reach[4 + facing], respectively. Do not assume native linker adjacency, clamp
the index, expand the first array with copied game data, or disable bounds
checks. Invalid facing/base values fail explicitly. Regression checks all
three bases and four facings against the actual source arrays. The original
matching expression remains unchanged. HOPS has a separate cross-table alias
which remains tracked and has not yet been reached by this replay.

The movement replay also reaches the original flattened facing_reach access
through row zero. Native builds select facing_reach[facing] directly, keeping
the same halfwords without pointer arithmetic beyond the first two-word row.

<a id="adr-036"></a>

## ADR-036: Implement bounded nonoverlapping VRAM moves

Date: 2026-10-26. Status: nonoverlapping/identical copies implemented.

The New Game scene animation calls MoveImage. Execute nonoverlapping transfers
directly on the game thread in the existing VRAM model, including 10/9-bit
coordinate wrapping, zero-as-maximum extents, mask checks/sets and dirty tiles.
MoveImage2 uses the same synchronous transfer path. Identical coordinates are
valid and still apply mask rules. No copy allocation is required.

Nonidentical overlapping source/destination rectangles fail explicitly, with
wrap-aware intersection detection. The public hardware transfer description
does not establish their internal copy order, so do not invent snapshot or
memmove behavior. Tests cover wrapped sources, mask preservation, identity
copies and the overlap diagnostic. This restriction must be resolved if an
actual game path requires overlapping transfers.

Reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/,
GPU memory transfer commands and mask-bit settings. No SDK code is used.

<a id="adr-037"></a>

## ADR-037: Apply axis rotations to the existing matrix

Date: 2026-10-26. Status: deterministic reference; SDK quantization pending.

The real-disc path reaches RotMatrixY in camera_frustum_setup. Supply X/Y/Z
helpers by left-multiplying the existing rotation matrix by the requested axis
rotation. Preserve translation and use an alias-safe intermediate. Reuse the
integer Euler path rather than adding a separate sine or rounding policy.
The test composes Y then X then Z and compares the resulting noncommuting
quarter turns with the documented ZXY Euler order. SDK product quantization
remains unverified. Remove the corresponding test-only projection traps now
that real implementations exist.

Reference: public Run-Time Library Reference 4.7, RotMatrixX page 8-120,
https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf (axis matrix * m).

<a id="adr-038"></a>

## ADR-038: Yield at the event loader's busy CD poll

Date: 2026-10-26. Status: accepted deterministic scheduling adaptation.

The non-sanitized Windows release stalls during New Game after tick 4800.
A native debugger backtrace identifies event_script_load_wait looping in
afile_read_bytes/fade_is_settled. The original CD progresses asynchronously;
the native runtime advances transfer credit only at deterministic VBlanks.
This loop can poll forever without returning to a VSync call.

Only in native builds, yield one VBlank when a script read is pending and the
fade is not settled, immediately before repeating that busy loop. This services
CD transfer credit, input, audio and original VBlank callbacks on the same game
thread. Preserve nonblocking CdReadSync semantics and the original matching
loop. Do not advance CD data on arbitrary status polls or a host background
thread. A local release replay then enters the original grandparent scene
and dialogue instead of hanging. Longer input/scene coverage remains pending.

The flashback's foreground load reaches the second empty polling loop in
event_script_start, confirmed by an attached native debugger. Yield one native
VBlank in that loop as well. Both waits service the same hardware tick path.

Returning from the flashback reaches a sound-bank wait in
farm_apply_pending_mode. Its repeated sound_load_tick calls also need the
CD tick to complete the bank. A native debugger confirms this third spin.
Yield while the bank remains pending, preserving the ordinary per-frame
sound driver and the matching loop.

<a id="adr-039"></a>

## ADR-039: Handle an absent terrain primitive on native hosts

Date: 2026-10-26. Status: native safety adaptation; low-memory parity pending.

The extended opening replay changes to area 230. A native debugger catches
player_frame_update reading address zero in its terrain-result helper. The
ground query can return no primitive, but the source unconditionally reads
its first word. player_update_position contains the same unconditional read.

For native builds, an absent primitive supplies no terrain flags (word zero).
Keep the first helper's existing 0x8000 fallback when no high flags are present;
the position path retains the zero word. Read actual primitive words with the
fixed-width byte accessor. Keep both original dereferences in matching branches.
This lets the release replay enter the original flashback scene; it does not
establish the contents/effects of original low-memory reads.

<a id="adr-040"></a>

## ADR-040: Return native event-request pointers explicitly

Date: 2026-10-26. Status: accepted for native calling-convention correctness.

The release replay runs the introductory script while the sanitizer replay
clears New Game and enters the farm without it. Both event_request_claim
wrappers call scene_claim_entry but omit return despite declaring a pointer
result. Callers require that pointer to populate script id/flags. Relying on a
callee's residual return register is undefined in native C and instrumentation
can change the result. These omissions are visible compiler warnings.

Native branches explicitly return scene_claim_entry's pointer. Keep the original
call-only bodies for matching builds. A regression calls both original wrappers
and checks returned slot identity, zeroed arguments, duplicate rejection, all
five queue entries, full-queue failure and reuse after reset. Do not suppress
the warning or force a guessed success pointer. Other return-type warnings
remain tracked for their own call paths.

<a id="adr-041"></a>

## ADR-041: Decode the source disc's XA streams on the game thread

Date: 2026-10-26. Status: deterministic reference; hardware audio parity pending.

The extended opening reaches cd_audio_play_track and requests Setfilter/ReadS.
A local header-only scan of all four XA files confirms file number 1, channels
0..7, and coding byte 1 (37.8 kHz stereo, 4-bit ADPCM) for every audio sector.
Unused sectors and per-channel EOF records are present. No audio/game data is
added to git. Other XA coding modes remain explicit failures.

Reuse the port's own tested SPU predictor decoder by rearranging each XA sound
unit's nibbles into a bounded temporary block. Keep independent channel history.
Resample each six decoded stereo frames to seven output frames using the public
29-tap/seven-phase zigzag coefficient facts. Reset decoder history at ReadS.
The public reference itself notes uncertain hardware rounding and startup phase;
this is a reproducible reference, not proof of bit-exact CD audio output.

Advance physical sectors at the existing 75/150-sector deterministic tick rate,
apply file/channel filtering, queue selected PCM in a fixed 8192-frame game-thread
ring, and drain queued audio after selected-channel EOF. Standby/cancel/data reads
clear the stream. GetlocP returns an eight-byte position response without a
four-byte write into its final three-byte position field. No audio worker or
per-frame heap allocation is introduced.

Mix decoded PCM through the source-used serial volume/mix controls before mono
and master gain, with saturation. Exact SDK volume law and CD reverb parity are
unverified. Synthetic tests cover literal resampler impulse values, predictor
history, coding rejection, filter/EOF selection, position response bounds, PCM
mixing, muting and cancellation. Real-disc continuation remains under test.

References: https://psx-spx.consoledev.net/cdromformat/ (XA headers, predictors,
interpolation coefficients) and public CD command descriptions. No SDK or other
PSX implementation code is copied.

<a id="adr-042"></a>

## ADR-042: Pass the harvest helper to its action callback

Date: 2026-10-26. Status: accepted for native calling-convention correctness.

After Zack's introduction, the first unpaused field ticks run the harvest
helpers. ASan catches a write into read-only storage in farm_cue_step_run,
called through harvest_helper_timer_tick. That dispatcher calls action() with
no argument although the cue/work callbacks require the helper/actor pointer.
The original code relies on an argument register staying live; native code
cannot rely on it, and the wrong pointer reaches cue state writes.

Pass helper explicitly only in the native branch. Keep the original no-argument
call in the matching branch. All action-table entries consume the helper or
its first actor member. A regression runs the original public helper updater
twice with all seven helpers offscreen and clock ticking, checking each cue's
start/play state, timer, position integration and unrelated field preservation.
No helper is disabled or replaced with a success stub.

<a id="adr-043"></a>

## ADR-043: Keep storage slot and navigation access within native objects

Date: 2026-10-26. Status: accepted for native array bounds.

A local replay opens the original toolbox screen. Inspection of its input and
transfer path finds two original array-subobject crossings: item_row_at indexes
past player.held[2] to reach tools/items, and navigation indexes past the first
marker's 12-byte next_cell field to reach subsequent markers. Modern bounds
instrumentation cannot treat those distinct arrays as one object.

Native branches select player.tools/items explicitly for bag slots 1..8 and
the selected marker's own navigation field. Held and storage-grid routing stays
unchanged; no out-of-bounds bag address is formed for grid slots. Matching
branches retain the original flattened expressions. This preserves the exact
logical slot mapping, without changing inventory rules or item data.

The regression includes the original storage implementation and checks actual
held, bag and all storage-row pointer destinations for toolbox/cabinet/icebox.
It also asserts the 16-byte marker layout and four-byte navigation offset.
Real menu transfer and subsequent tool-use replay coverage is in progress.

<a id="adr-044"></a>

## ADR-044: Documents storage and persistent native memory cards

Date: 2026-10-26. Status: accepted by explicit owner storage request.

The owner chooses Documents/BackToNature/{saves/,settings.json} instead of the
default application-pref directory, and performs interactive gameplay tests.
Resolve Documents through SDL_GetUserFolder, respecting Windows folder
redirection and Unicode names. Never hardcode C:/Users/User or the local owner
account. The native window initializes storage; headless diagnostics remain
disconnected unless explicitly given a storage backend by a unit test.

Implement both ordinary memory-card slots using a native, versioned container
(saves/card1.hmc and card2.hmc), not a raw PS1 image. The container has an explicit
512-byte little-endian directory/header and fifteen 8192-byte data blocks.
Each 32-byte directory record holds a zero-terminated 20-byte maximum name,
block count and starting block. Preserve the game's original file payloads,
allocation limit and read/write offsets. The original overlay requires read
completion ID 3 and write ID 4; complete requests deterministically on the game
thread. Ordinary cards report McxErrInvalid to PocketStation type queries, so
the original game selects its ordinary-card path.

The platform writes a sibling temporary file, flushes/closes it and renames
it over the destination. Failed writes leave the old container and committed
RAM state intact. This reduces partial-write risk; power-loss durability has
not been established. Validate header/version, names, sizes, allocation bounds,
duplicate names and overlapping block ranges on mount. Corrupt or unreadable
existing cards are reported as invalid and never automatically reformatted.
Explicit in-game format/unformat requests replace only the selected card.
Windows holds an exclusive file handle on .lock for the session to prevent
concurrent writers. Additional platform locking remains future platform work.

Create settings.json with schema_version 1 only if missing. Preserve an existing
file byte-for-byte; reading/applying settings is a separate pending capability.
No game option or enhancement is introduced here. All SDL and Windows filesystem
APIs remain in platform/. No decomp source or new dependency is changed.

Tests cover native card capacity, slots, directory patterns, range errors,
completion IDs, reopening serialized bytes, failed-write preservation and corrupt
input. An SDL filesystem test uses a unique build-local Unicode directory for
actual file reopening and existing-settings preservation. The owner validates
the real save/load menus; storage unit coverage does not establish that the
entire original game serialization or overlay reentry works.

<a id="adr-045"></a>

## ADR-045: Yield during save/load asset waits and own the complete save image

Date: 2026-10-26. Status: accepted for native save/load bring-up.

The owner reports an unresponsive window after selecting Load at the title.
Attaching LLDB to that existing process finds the game thread in CdReadSync,
afile_read_bytes and load_screen_tick. Detach after collecting the stack;
no gameplay input or new game/replay session is launched. Both save_load_payload
and save_load_image_file poll asynchronous reads in tight loops without VSync.
The native CD model advances only at deterministic VBlank points, which also
pump the window host's events. Yield one VBlank only while each read is pending.
Keep both original matching loops unchanged in their non-port branches.

The same source declares a 0x80-byte header followed by three 0x80-byte icon
frames, a 0x80-byte slot record and a payload ending at byte 0x9690. It reads,
writes and clears the combined span through the header. Native globals need
not be adjacent, and sanitizer redzones make that access invalid. Provide one
native union containing the complete byte image and those same typed slices;
assert total size and the 0x80, 0x200 and 0x280 offsets. Retain the original
separate globals in the matching branch. This changes native workspace ownership,
not the on-card file format or serialization rules.

The overlay's save_card_poll explicitly expects command 1 from MemCardExist.
Correct the native existence completion from its previous opaque value 2 to 1;
otherwise the menu stays busy even after a successful existence query.

An isolated test includes the original save/load implementation. Its bounded
asynchronous asset fixture completes only after two real native VSync ticks,
and fails quickly if the original busy polling returns. Rendering sinks capture
the expected texture and five sprite bindings without parsing game assets.
The test also runs save_reset_state, verifies the contiguous image slices,
and exercises the original probe/poll helpers on both synthetic ordinary cards.
Storage regressions verify the existence completion ID. No real save files,
gameplay session or owner Documents directory is accessed by these tests.
In-game confirmation remains with the owner. Matching check is unavailable on
this native Windows host; text reconstruction of matching branches is separate.

<a id="adr-046"></a>

## ADR-046: Give menu background callbacks an exact native function type

Date: 2026-10-26. Status: accepted for native indirect-call correctness.

The shared menu background dispatcher calls a TileDrawFn taking s16 frame and
s32 coordinates. Its two original callbacks instead declare int/short or
short/short parameters. Earlier toolbox sanitizer diagnostics reported this
incorrect indirect function type; save/load screens use the same dispatcher.
Keep the original callbacks and use native adapters with the exact dispatcher
signature, explicitly narrowing coordinates as the original callbacks do.
Do not suppress function-type sanitization or change matching callback selection.

A regression calls both original public background modes twice, including
the fourth-call scroll increment. Capturing drawing sinks check all 165 tiles
per call, frame sequence, negative coordinates, animation binding and current
frame primitive-cursor pointers. No actual asset or window is used. The original
matching branches reconstruct exactly; the matching compiler check itself
remains unverified because its Linux toolchain is unavailable here.

<a id="adr-047"></a>

## ADR-047: Restore title and save/load overlays from native object snapshots

Date: 2026-10-26. Status: accepted for the title/load/return lifecycle.

The owner reaches the Load menu, then returning to title aborts at the explicit
overlay reentry guard from ADR-017. Removing that guard alone would retain
mutated overlay data and BSS. Capture pristine native object representations
before hm_game_main, after native relocations and C initializers have run.
Before each supported activation, copy them back into their original objects.
Resident game state, work buffers, hardware state and the other overlay region
are not included in this restoration.

The title inventory contains all 44 writable file-scope objects; save/load has
42 original writable objects plus the native save workspace. Clang AST inspection
and source review found no function-local static variables in these two units.
Keep immutable constants untouched. Lists live in native-only includes at the
end of each original translation unit and must be maintained when writable
overlay state changes. This preserves private symbols and captures initialized
tables, pointers, callbacks and zero-initialized arrays without assuming object
adjacency, PS1 address layouts, linker sections or disabled sanitizer redzones.

The registry copies object descriptors and pristine bytes once at startup.
Repeated capture calls cannot replace the pristine copy with mutated state.
Registry metadata reset retains snapshots. Unsupported overlays still explicitly
reject reentry; their state inventories are not silently assumed complete.
This supersedes ADR-017's reentry restriction only for title and save/load.

Tests mutate actual private title counters, callback, arrays, native pointers and
initialized tables and verify five restorations. The save/load fixture verifies
four restorations including its complete workspace and animation pointer. A
separate integration unit registers both real inventories, alternates their
selections six times and checks resident game state survives. No game loop,
window, disc or interactive replay is started. Matching check remains unavailable
on this native Windows host; matching-branch text reconstruction is separate.

<a id="adr-048"></a>

## ADR-048: Pause at the host boundary with private original-game UI resources

Date: 2026-10-26. Status: initial interface implemented; advanced rendering pending.

The owner requests an ESC interface across scenes with Resume, Settings >
Graphics and Exit Game, using the game's own UI presentation. Complete the
title/load reentry fix first, merge PR #1 as windows port init, remove old
branches and create port/windows-pause-menu from the latest main. The new
feature work is separate from that merged Windows foundation.

Pause inside the SDL host VBlank boundary. Keep pumping OS events and drawing
the private menu without returning to VSync, CD, SPU or the game callback.
Pause/resume the audio device, retain its sample queue, and shift the pacing
epoch by elapsed pause time. Suppress held confirm/pad input until released
when returning to the game. The UI navigation state machine is data-free and
does not touch the game's dialog state, current frame, ordering tables or RNG.

Read the resident UI bank from A_FILE through the independently validated disc
parser before game entry, decode with the original texture-pack function,
cache its original 8x12 English font and text palette, and reset startup VRAM.
The cache remains private across all overlay transitions. Draw the actual
window_frame.c geometry through private pixel/line/gradient sinks. New labels
use the game's glyph mapping. Do not bundle extracted assets or substitute a
host font, generic UI toolkit or invented bitmap art.
The original primitive builders consume packed 0xRRGGBB; do not follow the
contradictory older 0xBBGGRR comment in window_frame.c. The owner reported a blue
initial prototype, caused by reversed red/blue channels in the private canvas.
Correct that conversion and retain the original cream gradient. Synthetic
checks assert the expected cream RGB channels and a red glyph independently.
Use compact centered root/settings panels and distinct Graphics header, aligned
label/value rows, selection frame and detail footer. Asset-only previews are
written under ignored build paths for visual inspection, never into git.

SDL platform code enumerates monitor display modes, applies windowed/exclusive/
borderless modes and offers nearest or bilinear final-frame filtering. Preserve
4:3 in the active resolution with black bars. Borderless uses the desktop mode.
The graphics object in settings.json is atomically replaced while preserving
other root properties. Reject malformed JSON rather than overwrite it. The
owned object stores monitor ordinal, window_mode, width, height and bilinear.
Unavailable stored monitor/mode choices fall back to a valid display or default
window. Multi-monitor hotplug and display switching still require owner testing.

HMBTN_ENH_PAUSE_MENU remains OFF by default in CMake; Windows -Game enables it
for the owner's explicitly requested globally available interface. Its runtime
default is available, with enhance.pause_menu=false as an explicit OFF path.
AGENTS.md records this narrow override. Native scene scaling, expanded camera/
culling and FXAA/MSAA are separate pending render paths. Their menu entries
remain OFF and unavailable. Bilinear final-frame presentation is not advertised
as native higher-resolution rendering or per-texture filtering.

Synthetic tests cover navigation, original dialog-frame geometry with synthetic
glyphs, JSON editing/validation and preservation of unknown settings. A local
asset-only probe decodes the owner's font and checks VRAM is reset, without
opening a window or entering the game. The owner performs interactive pause,
audio, fullscreen and gameplay validation. No new dependency or decomp source
change is required for this initial interface. Performance budgets and rendering
enhancement parity remain unverified.

<a id="adr-049"></a>

## ADR-049: Independent rational presentation limits and VSync control

Date: 2026-10-26. Status: implemented; owner device/gameplay tests pending.

The owner requests VSync On/Off and Native or monitor-derived FPS limits without
changing game time. Original main_run_frame uses VSync(2): 30 normal updates
at the port's fixed 60 Hz hardware tick. Keep this source, psx/timing.c, CD,
SPU, RNG and game callbacks unchanged. During each host tick wait, service a
separate presentation clock and pump OS events. Sample input at the tick
boundary. No new game-logic thread or variable simulation step is introduced.

Accumulate absolute rational nanosecond deadlines, preserving 60000/1001 rather
than truncating to 59 or 60. Drop overdue presentations instead of submitting
catch-up bursts. Display settings reset only presentation scheduling; paused
wall time remains excluded from the game epoch. Native caps presentation at 30.
Other choices come from the monitor's desktop-resolution modes when windowed/
borderless, or selected resolution when exclusive. Persist fps_numerator and
fps_denominator in graphics; zero numerator means Native. Unsupported saved
rates revert to Native. An FPS cap does not itself change desktop refresh.

On uses synchronized mailbox if available, otherwise FIFO. Off requires
supported immediate presentation; reject unsupported changes and retain the
effective setting. An unavailable stored Off setting falls back to On. Bound
VSync-On presentation by the current physical refresh and recompute on display
changes. Acquire through SDL's non-waiting frame-fence path; cancel when no
swapchain image is available and retain two frames in flight. SDL 3.2.28 Vulkan
WSI acquisition can still wait internally, so no zero-driver-stall guarantee
is made. Hardware stalls and achieved FPS remain owner device checks.

Higher caps repeat the latest native image. They add no game update, animation
advance, RNG call or audio generation. They do not provide 75 unique scene
frames or interpolation. Graphics now has nine items with seven visible rows
and scrolling. Decimal rates use the original narrow dot glyph, verified from
a private asset-only atlas; no extracted font is committed.

A synthetic regression checks 200000 rational presentation boundaries per
rate and verifies 300 real native VSync(2) calls still give 600 VBlank callbacks
at 30, 59, 60, 75, 144, 240 and 60000/1001 caps. Late dropping, menu scrolling
and decimal composition are covered. These are technical checks, not gameplay
time or physical VSync tests. No dependency or matching-source edit is added.
