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
Original source headers remain unmodified.

## Repeatable lexical audit

`port/analysis/source_audit.py` scans source and headers without any SDK input.
It writes deterministic JSON with source/line evidence for includes, ordinary
function definitions, external call candidates, GTE invocations, and overlay
regions. The initial run reports:

| Item | Count |
| --- | --- |
| C sources / headers | 113 / 110 |
| Ordinary function definitions, including static functions | 2627 |
| Unique ordinary external function names | 950 |
| Duplicate ordinary external function name candidates | 0 |
| External call-like names requiring classification | 299 |
| GTE-prefixed invocation names | 40 |

This is not a preprocessor or a C semantic analysis. Macro-generated functions
(including farm courtship helpers), callbacks, type casts, declarations, global
data definitions and conditional compilation require compiler/linker analysis.
The 299 candidates are not 299 confirmed SDK functions; the 40 GTE-prefixed
names also include game helpers. Six regression fixtures cover comments,
strings, macro continuations, initializer braces, prototypes, operators and
source-line preservation, collision filtering and overlay ownership. Generated
reports stay in ignored `build/`.

## Initial native source compilation

`hmbtn_game_initial` compiles six original, unmodified units:
`main/geometry/math_util.c`, `main/game/farm_data_size.c`,
`main/game/horse_names.c`, `main/game/picture_codes.c`,
`main/game/event_flags.c`, and `main/game/farm_tables.c`.
The port force-includes `compat/game.h`: fixed-width primitive types and scoped
SDK integer aliases. Minimal VECTOR/SVECTOR/CVECTOR/MATRIX and GPU packet
declarations follow source field usage and public GP0 formats (ADR-006).
GPU packet sizes/offsets and a known textured-quad byte sequence are checked;
GS ordering-table descriptors retain native pointers. No GPU execution or OT
address mapping is implemented. Original LINK_PRIM still truncates host pointers
and must not be used as a native runtime until that issue is resolved.

The four GTE calls (`rsin`, `rcos`, `SquareRoot0`, `VectorNormal`) abort with their
names in all builds until faithfully implemented; they are not numerical
approximations. Eight tests cover type/endian and layout assumptions, original integer power,
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
No main loop, title screen, or renderer is linked yet.

An earlier `llvm-nm --defined-only --extern-only` pass confirmed six math functions
and four data symbols in the initial four-unit subset. The current syntax-only
probe of `main/main.c` passes the new libetc declarations, then stops at missing
`libcd.h`; further SDK headers and the main-loop dependencies remain unavailable.

Warnings retained in `math_util.c`: unused static functions `vec3_add`,
`vec3_differs`, `vec3_move_toward`, `math_cross2_sign`, and
`vec_dot_normalized`. These are tracked, not suppressed or removed.
`farm_tables.c` adds 22 missing-braces warnings for the existing crop table and
two incompatible function-pointer casts at lines 733 and 735. The latter adapt
`item_use_coin` and `farm_adjust_stamina_fatigue` to ItemUseFn; calling these through
that type needs a deliberate port solution before item actions are enabled.

## Full-tree native syntax inventory

`port/analysis/compile_probe.py` invokes Clang on all original C files with the
port force-include and the basename include directories. It writes per-file
diagnostics to ignored `build/port/compile-probe.json`. No file is changed and no
game code is linked or run. The result with Clang 20.1.8 is 55 syntax passes and
58 failures out of 113 files, with 1228 warnings emitted before the error cap.

Common blockers include missing libcd/libapi/abs headers, undeclared GPU API
calls, MIPS register clobbers, SDK helper constants, GCC nested functions and
macro-generated declarations not accepted by modern C. The probe's five-error
limit means this is not a complete error count. Syntax passes can still fail
during code generation, linking, runtime layout checks or sanitizer execution.

Warnings include pointer-to-integer/integer-to-pointer truncation, excess and
missing initializers, incompatible callback types, and a transposed memset
argument warning. They remain visible; broad warning suppression or automatic
source cleanup would hide real fidelity and 64-bit hazards.

## Port guards and validation

No `HMBTN_PORT` guards or original source edits have been introduced.
The port now builds the six game units above, not a game executable.
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
