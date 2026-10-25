# Native port reconnaissance

Status: Phase 0 in progress, 2026-10-25. This is a source inspection, not a
successful native boot or a verified matching baseline.

## Source and overlays

The tree contains 113 C files and 110 headers. `src/main/` contains the resident
program, with `cd`, `game`, `geometry`, `graphics`, `input`, `sound`, `text`, and
`ui` subdirectories. `src/include/` contains shared types, actor/game layouts,
GTE macros, CPU assembly helpers, and C-library declarations.

The root CMake project links the EXE and all overlays in one linker invocation.
See `cmake/psx.cmake`, `cmake/program.ld.in`, and each overlay's CMakeLists.txt.

| Overlay | Link region |
| --- | --- |
| 08_DEBUG | low |
| 10_PdaSamp | low |
| 11_GF_FARM | low |
| 15_saveload | high |
| 17_GF_MG1 | low |
| 19_GF_MG2 | low |
| 21_GF_MG3 | low |
| 23_GF_MG4 | low |
| 25_GF_MG5 | low |
| 27_GF_TITLE | low |
| 28_GF_STAFF | low |
| 30_GF_swind | high |
| 31_gf_mcard | low |

Low starts at `0x800B7E20`; high starts at `0x8012DDE8`. These are matching
link addresses, not host allocations. `src/main/overlay_regions.c` provides
the destination table. `main.c`'s overlay read macro delegates to
`afile_read_range`; the farm can coexist with a high-region overlay. Replacing
only the disc read is insufficient: dispatch, copied tables, and address
arithmetic must also be audited before static host linking.

Global symbol collisions have not yet been established using compiled overlay
objects. The repeatable lexical audit below found no duplicate ordinary external
function definitions, but excludes macro-generated functions and global data.
Do not prefix every symbol blindly: resident imports must remain shared.

A local Clang 20.1.8 object-generation probe now succeeds for the 87 units that
pass syntax checking. Its LLVM nm inventory finds no duplicate external source
definitions in that subset; the 19 repeated .refptr names are compiler-generated
COFF references, not game definitions. There remain 289 undefined names outside
the subset, including missing game units, SDK APIs and host C runtime imports.
The remaining 26 units and an actual whole-program link must still be checked
before deciding the final overlay namespacing map.

## SDK surface

`sdk/objects.txt` selects objects from LIBAPI, LIBC, LIBCARD, LIBCD, LIBETC,
LIBGPU, LIBGS, LIBGTE, LIBMATH, LIBMCRD, LIBMCX, LIBPAD, LIBSN, LIBSND and
LIBSPU, plus 2MBYTE.OBJ. This is a link-input inventory, not proof every
function in these libraries is called by the game.

Observed calls include:

| Area | Examples and source evidence |
| --- | --- |
| CD | `CdSearchFile`, `CdControl`, `CdRead`, `CdReadSync`, `CdIntToPos` in `main/cd/afile.c` |
| Timing/GPU | `ResetCallback`, `DrawSync`, `VSync` in `main/cd/afile.c` |
| Audio | `SpuSetCommonAttr`, `SpuGetAllKeysStatus`, `SsSeqOpen`, `SsSeqPlay`, `SsSeqStop`, `SsSeqPause`, `SsSeqReplay`, `SsSeqSetVol`, `SsSeqClose` in `main/sound/sound.c` |
| GTE | `gte_rtps`, `gte_rtpt`, `gte_nclip`, `gte_avsz3`, `gte_avsz4`, `gte_sqr0`, `gte_op12` definitions in `include/gte.h` |

A lexical call/macro inventory is now available; an undefined-symbol comparison
of the complete program is still required. No SDK header contents have been
copied into the port.

## Audio and video streams

XA streaming is confirmed: `main/sound/cd_audio.c` searches `STR/01.XA` through
`STR/04.XA`, selects file/channel, and drives playback with `CdlReadS`.
`sound.c` also opens SEQ data and VAB-related sound resources. A source search for
`DecDCT`, `StGet`, `StSet`, `CdPlay`, and `CdlPlay` found no occurrences.
This does not prove the absence of all FMV or CDDA paths; data inspection and
a full call inventory remain pending.

## Assembly, addresses, and 64-bit hazards

- `include/cpu_asm.h`: MIPS scratchpad stack switching, return-address stores,
  and `break 1024`. Scratchpad is `0x1F800000`, size `0x400`.
- `include/gte.h`: raw coprocessor words via `__asm__`, plus `inline_c.h`.
  These local headers redefine macros, so an SDK shim alone cannot replace them.
- `main/main.c:522`: overlay pointer cast to `s32` before address arithmetic.
- `main/graphics/anim.c:40`: pointer cast to `u32` for alignment checking.
- `main/sound/sound.c:429`: pointer cast to `s32` to compare a sentinel.
- `main/sound/sound.c:430`: writes a sequence handle through an aliased `s32 *`.
- `u_long` in SDK-facing reads must remain a 32-bit PS1 word on LP64 hosts.

These are examples, not an exhaustive hazard list. Native compilation and
sanitizer coverage of game code have not started. Suspicious original behavior
must remain unchanged until its meaning is established.

## Asset layouts

- `main/cd/afile.c`: 33 archive records, including 13 overlays and a sentinel;
  sector-rounded reads and offset tables. `tools/afile.py` describes archive IO.
- `main/graphics/anim.h`: ANI section offsets, image rectangles, part lists,
  ranges and frames. `AnimFile` has five 32-bit offsets after its header.
- `main/graphics/texture_pack.h`: type `0x0D` image banks; 12-byte image header,
  24-bit byte size plus flags, and variable-length word-padded image payloads.
- Save/card layouts, other asset structures, and embedded pointer relocation
  require a separate inventory before runtime loading.

Initial pointer-free asset layout checks now live in `port/tests/layouts.c`.
Original asset declarations remain unmodified; the native main.h pointer getter
guard is listed below.

## Repeatable lexical audit

`port/analysis/source_audit.py` scans source and headers without any SDK input.
It writes deterministic JSON with source/line evidence for includes, ordinary
function definitions, external call candidates, GTE invocations, and overlay
regions. The initial run reports:

| Item | Count |
| --- | --- |
| C sources / headers | 113 / 110 |
| Ordinary function definitions, including static functions | 2628 |
| Unique ordinary external function names | 950 |
| Duplicate ordinary external function name candidates | 1 |
| External call-like names requiring classification | 301 |
| GTE-prefixed invocation names | 40 |

This is not a preprocessor or a C semantic analysis. Macro-generated functions
(including farm courtship helpers), callbacks, type casts, declarations, global
data definitions and conditional compilation require compiler/linker analysis.
The 301 candidates are not 301 confirmed SDK functions; the 40 GTE-prefixed
names also include game helpers. Six regression fixtures cover comments,
strings, macro continuations, initializer braces, prototypes, operators and
source-line preservation, collision filtering and overlay ownership. Generated
reports stay in ignored `build/`.
The single duplicate-name candidate is main_get_region_base's mutually exclusive
native/matching definitions in one file. This illustrates the audit's lack of
preprocessing; it is not an actual linked collision.

## Initial native source compilation

`hmbtn_game_initial` compiles eleven original units with native guards listed below:
`main/geometry/math_util.c`, `main/game/farm_data_size.c`,
`main/game/horse_names.c`, `main/game/picture_codes.c`,
`main/game/event_flags.c`, `main/game/farm_tables.c`, `main/game/farm_state.c`,
`main/graphics/frame_buffers.c`, `main/graphics/prim.c`, `main/main.c`, and `main/cd/afile.c`.
The port force-includes `compat/game.h`: fixed-width primitive types and scoped
SDK integer aliases. Minimal VECTOR/SVECTOR/CVECTOR/MATRIX and GPU packet
declarations follow source field usage and public GP0 formats (ADR-006).
GPU packet sizes/offsets and a known textured-quad byte sequence are checked;
GS ordering-table descriptors retain native pointers. GPU link handles now map
to full native pointers through a bounded registry (ADR-009). Native LINK_PRIM
and ot_insert_prim share this codec with SDK addPrim/AddPrim; packet layouts
stay fixed-width. Rasterization, DrawOTag and complete GPU execution remain absent.

The four GTE calls (`rsin`, `rcos`, `SquareRoot0`, `VectorNormal`) abort with their
names in all builds until faithfully implemented; they are not numerical
approximations. The initial tests cover type/endian and layout assumptions, original integer power,
32-bit wrapping, interpolation/truncation/clamping, vector operations and each
stub's failure diagnostic. Static helper paths are not exercised by game.math.
`game.state` exercises all 1184 valid event flags, word/sign-bit boundaries,
native table registration across 19 estate runs, the skipRuns path, entry-state
reset and shared-work busy/free behavior. The test defines a host FarmState and
three explicitly test-only abort traps for item-action callback link dependencies.
These callbacks are never invoked in the test and are not runtime implementations.
Their original type-mismatched casts remain tracked below. Save serialization
and pointer-bearing state layout parity have not been tested.

`compat.layouts` checks A_FILE records, ANI headers/images/parts/ranges/frames,
image bank headers, input replay records and estate rows. ANI frames are 10 bytes
in the original declaration, with control at offset 7. No source edit was needed.
No main loop, title screen, or renderer is linked into an executable yet.

An earlier `llvm-nm --defined-only --extern-only` pass confirmed six math functions
and four data symbols in the initial four-unit subset. A new pass confirms
`hm_game_main` and `afile_init` in the archive. main.c is renamed only through
its port build definition; matching branches are preserved. These objects
are not selected by the current test executables. The game's hardcoded
WORK_REGION and frame table addresses now select a native data workspace, and
the resident region helpers retain full pointers (ADR-008). Main-loop runtime
dependencies, full GPU execution and overlay dispatch remain unsafe/unimplemented;
the complete game entry point must not be executed yet (ADR-007).

The new source-facing libcd/libapi/libmcrd/libmcx declarations and the narrow
GPU/GS/GTE startup declarations permit native entry-point code generation.
Declarations alone do not implement callbacks, rendering, cards or GS work memory.
The game force-include now supplies string.h as well: the host's memory.h alone
does not declare memmove, unlike the original compilation environment.

Warnings retained in `math_util.c`: unused static functions `vec3_add`,
`vec3_differs`, `vec3_move_toward`, `math_cross2_sign`, and
`vec_dot_normalized`. These are tracked, not suppressed or removed.
`farm_tables.c` adds 22 missing-braces warnings for the existing crop table and
two incompatible function-pointer casts at lines 733 and 735. The latter adapt
`item_use_coin` and `farm_adjust_stamina_fatigue` to ItemUseFn; calling these through
that type needs a deliberate port solution before item actions are enabled.
The four pointer-to-s32 warnings in main.c are removed by native-only guards;
its four unused-function warnings remain.
afile.c adds 33 missing-pos initializer warnings for the existing A_FILE member
table, two unused functions and two unused parameters. No warnings were suppressed.
The truncated primitive-address warning in frame_buffers.c is removed by its
native link guard. Its unused debug hook parameter warning remains. The new
gpu.links test exercises original ot_insert_prim with native pointers.

## Native workspace validation

The aligned 0xCF1E0-byte C workspace holds the 0x49000 image window, 0x47390
section/free capacity and 0x3EE50 frame work region. It is not a PS1 RAM image;
code, other globals, overlays and heap allocations remain outside it. Its offsets
are derived from the matching layout, not an increased memory budget (ADR-008).

`game.work_memory` includes original main.c instead of copying its functions,
makes the unused game entry local, and calls the actual normal/wide frame setup,
frame begin, GS work allocator/release and region accessors. It also links and
calls original farm_state_init and frame_buffers_layout. Test-only GPU/GS functions
record which OT regions are cleared and which work base is selected; they do not
encode OT links or render. Unexpected host-debugger/system-error paths abort.

Both layouts keep OT, primitive and GS work spans inside the native frame object.
The demo buffer is correctly aligned below region_base, and its recording survives
a frame-area reset. Original table registration and farm buffer pointers bind
without subtracting addresses of separately linked overlay objects. Free-space
checks cover zero use, a partial section, exact exhaustion and a negative budget
one byte past capacity, preserving the signed vanilla result rather than clamping.
Release and ASan/UBSan pass. Full scene loads, raw asset pointer relocation and
overlay region copies remain outside this coverage. GPU links are covered by
the separate test below.

## Native ordering-table links

`psx/gpu_links.c` stores full pointers under nonzero 24-bit handles and preserves
the top packet-length byte. 0xFFFFFF remains the end marker; zero and unknown
handles fail instead of becoming host addresses. A fixed registry supports
65536 live tag pointers without per-frame allocations; reset requires discarding
every cached chain first. Tag lifetimes and payload extents remain the caller's
responsibility. This is not asset-pointer relocation or an untrusted chain parser.

`psx/gpu_ot.c` implements reverse ClearOTagR, setlen, addPrim and AddPrim. The
generic add API preserves the head length; the guarded source helpers reproduce
the original OT-specific behavior of clearing that length. Original primitive
builders now compile and are linked into tests. ClearOTagR's return value is not
used by the game; validation covers its chain side effects.

`gpu.links` tests two live tag addresses with identical low 24 address bits,
identity reuse, relocation-independent handle order, reverse OT traversal,
LINK_PRIM length/value behavior, and original tile/rectangle/draw-mode bytes.
It calls original frame_buffers_layout and ot_insert_prim, checks upper-index
clamping and cursor advances, traverses a 2050-node chain, and verifies OT
re-clearing preserves cached IDs. GS allocation in the builder test is an
explicit test pool; its real source implementation is covered separately.
Six rejection cases cover null/unaligned tags, zero/unknown handles, exhausted
capacity and empty OT lengths. Capacity validation also checks reuse at the limit.
No renderer, GPU synchronization or overlay pointer restoration is tested.

A small optional benchmark measures only warm OT construction; measured setup,
numbers and limitations are in PLATFORMS.md. There is no full-game performance
or parity claim from these CPU packet tests.

## CD bring-up surface

`psx/cd_position.c` implements CdIntToPos/CdPosToInt from public BCD MSF addressing
rules, without a disc or SDK code. Known vectors cover lead-in, frame/second/minute
boundaries, a multi-minute location and the upper two-digit-minute location.
An exhaustive round trip covers 450000 positions (LBA -150 through 449849).
The reserved fourth byte is preserved. A separate eight-byte GetlocP response
fixture passes its final three bytes to CdPosToInt under ASan; the helper must
not read a fourth byte. Malformed BCD and out-of-domain input behavior is not
claimed as validated PSY-Q parity.

Disc access now uses a platform-owned positional stream, verified against the
original raw BIN SHA-1 before mounting ISO-9660. The parser checks directory
record bounds, both-endian extents/sizes, nested paths and track boundaries.
Mode 1 and Mode 2 Form 1 supply 2048-byte data blocks; Form 2 is rejected by the
filesystem reader rather than truncated. No SDK or game bytes are included.

CdInit, CdSearchFile, Setloc/Setmode/Pause/Standby, CdRead, CdReadSync, CdSync,
CdReadBreak and CdFlush have a single-threaded data-read implementation. Every
SDK entry requires an attached validated disc. Eight rejection tests cover
missing-disc failures. Nonblocking reads advance only through hm_cd_tick at
75/150 sectors per second on a 60 Hz tick. Blocking ReadSync drains the pending
transfer. Seek latency, XA commands/filtering, controller retry timing and full
hardware status behavior are not yet implemented or parity validated.

The local hmbtn_disc_probe executable validated the owner's original USA track
(90967 raw sectors), found SYSTEM.CNF, SLUS_011.15, A_FILE.HDT and A_FILE.BIN,
and read their initial sectors through both the parser and SDK-facing CD API.
All 33 A_FILE offsets (32 member ranges) are monotonic and inside the
133935104-byte archive. The probe is a diagnostic, not a game executable.
No assets were extracted or added to git. The source's afile_init has not run.

There are 32 CTests, all passing in Windows release and ASan/UBSan. New coverage
includes SHA-1 published vectors, malformed synthetic sectors/directories,
cross-sector file reads, deterministic CD transfer/cancel/error behavior,
VRAM transfers/masks/wrapping and the original texture-pack decoder. Tests need
no disc. The separate local disc probe also passed under ASan/UBSan.

The scalar inline GTE core now implements SQR, OP, MVMVA (source-used matrix and
translation selectors), NCLIP, AVSZ3 and AVSZ4 with explicit signed shifts,
44-bit accumulator wrapping, MAC0 overflow, IR/OTZ saturation and flag summary.
Literal formula vectors cover signs, limits and summary-bit exclusions. This
core is not yet connected to inline_c.h or the SDK entry points. Perspective
projection and the unused MVMVA hardware-bug selectors fail explicitly; no
hardware capture or full-game GTE parity is claimed.

CdlFILE is host-side result metadata with only the fields used by the source,
pos and size. It is never read from disc bytes and does not claim the proprietary
SDK's binary layout. CD command constants cover only commands observed in source.

## Full-tree native syntax inventory

`port/analysis/compile_probe.py` invokes Clang on all original C files with the
port force-include and the basename include directories. It writes per-file
diagnostics to ignored `build/port/compile-probe.json`. No file is changed and no
game code is linked or run. The result with Clang 20.1.8 is 87 syntax passes and
26 failures out of 113 files, with 1738 warnings emitted before the error cap.

Common blockers include missing libsnd and other SDK headers, undeclared GTE/font API
calls, inline GTE commands, SDK helper constants, GCC nested functions and
macro-generated declarations not accepted by modern C. The probe's five-error
limit means this is not a complete error count. Syntax passes can still fail
during code generation, linking, runtime layout checks or sanitizer execution.

Warnings include pointer-to-integer/integer-to-pointer truncation, excess and
missing initializers, incompatible callback types, and a transposed memset
argument warning. They remain visible; broad warning suppression or automatic
source cleanup would hide real fidelity and 64-bit hazards.

## Port guards and validation

Thirteen `HMBTN_PORT` guard blocks have been introduced under ADR-008 (workspace),
ADR-009 (GPU links) and ADR-010 (native CPU adapters):

| File | Guarded change |
| --- | --- |
| src/main/main.c | WORK_REGION selects native storage |
| src/main/main.c | main_region_gap_const subtracts pointers within that storage |
| src/main/main.c | main_region_free_bytes preserves the budget without overlay-address subtraction |
| src/main/main.c | main_get_region_base returns a full native byte pointer |
| src/main/main.c | Normal frame OT base uses its original offset |
| src/main/main.c | Wide frame OT base uses its original offset |
| src/main/main.h | Native region accessor has a pointer return type |
| src/main/game/farm_state.c | Overlay-address temporaries are matching-only |
| src/main/game/farm_state.c | Farm load window binds to native storage |
| src/main/graphics/prim.h | LINK_PRIM stores a native pointer handle instead of address bits |
| src/main/graphics/frame_buffers.c | ot_insert_prim uses the same native link codec |
| src/include/cpu_asm.h | Keep the native stack and use explicit diagnostic hooks |
| src/main/graphics/anim.c | Check alignment using the full host pointer |

For each of these seven files, selecting every matching `#else` branch and
removing the new guard directives reconstructs the previous source text exactly,
including its whitespace (line endings normalized for comparison). This check
passed against the preceding git revision. It does not establish byte-matching
compiler output. Overlay callers of main_get_region_base that still store its
result in s32/u32 now require explicit port fixes before those paths can compile.
The port now builds the eleven game units above, not a game executable.
Matching `check` is unverified: Ubuntu WSL failed to start with
`HCS_E_HYPERV_NOT_INSTALLED`. Project-local Windows tools were subsequently
provisioned: CMake 3.31.6 and Ninja 1.12.1. LLVM-mingw was upgraded from
20241001 (Clang 19.1.1) to 20250709 (Clang 20.1.8) after ASan failed during
startup on this host; see ADR-005. Windows release and ASan/UBSan tests are
recorded in PLATFORMS.md.
An Android NDK Clang 18.0.3 freestanding syntax check of the type test also passed;
this is not an Android build. No disc, full-game, or device tests were run.

Remaining Phase 0 gates: native matching baseline, complete SDK and layout
inventory, compiled overlay symbol collision map, broader sanitizer coverage, and reproducible
matching host-tool pins. The owner prioritizes Windows and excludes workflow/CI
work for now. Existing workflows remain unchanged.
