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
