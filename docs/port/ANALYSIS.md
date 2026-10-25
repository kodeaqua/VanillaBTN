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

Global symbol collisions have not yet been established using compiled objects.
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

A complete call/macro inventory and undefined-symbol comparison are still required.
No SDK header contents have been copied into the port.

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

No asset layout assertions have yet been added to original structs.

## Port guards and validation

No `HMBTN_PORT` guards or original source edits have been introduced.
The Phase 0 port only builds a fixed-width type/endian test, not game code.
Matching `check` is unverified: Ubuntu WSL failed to start with
`HCS_E_HYPERV_NOT_INSTALLED`. Project-local Windows tools were subsequently
provisioned: LLVM-mingw 20241001 (Clang 19.1.1), CMake 3.31.6 and Ninja 1.12.1.
Windows release configure/build passed; `compat.types` passed (1/1).
An Android NDK Clang 18.0.3 freestanding syntax check of the type test also passed;
this is not an Android build. No disc, game, sanitizer, or device tests were run.

Remaining Phase 0 gates: native matching baseline, complete SDK and layout
inventory, compiled symbol collision map, sanitizer runs, and reproducible
matching host-tool pins. The owner prioritizes Windows and excludes workflow/CI
work for now. Existing workflows remain unchanged.
