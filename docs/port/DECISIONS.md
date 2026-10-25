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
