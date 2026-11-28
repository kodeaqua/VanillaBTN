# VanillaBTN roadmap

VanillaBTN is a planned native source port of *Harvest Moon: Back to Nature* (PS1,
USA, `SLUS_011.15`) based on `hmbtn-decomp`. The repository has a matching PS1 build
and a Windows x64 startup executable.

## Project decisions

| Topic | Decision |
|---|---|
| Source and platforms | Share the decompiled C source. Start with Windows x64, then port to Linux and macOS. |
| Matching build | Preserve PSY-Q 4.7, GCC 2.95.2, compiler flags, linker layout, and artifact names. |
| Native build | Add a CMake target named `vanillabtn` and Debug and Release presets. PsyZ provides the PSY-Q API and uses an SDL3 backend. |
| Dependencies | Plan to use a submodule at `external/psyz`. Pin a tested PsyZ commit and use its SDL revision. |
| Game data | Import the user's USA BIN/CUE dump locally on first run. Keep game data and the PSY-Q SDK out of the repository and release packages. |
| Parity | Preserve gameplay, timing, graphics, audio, input, and save compatibility. Enhancements and quality-of-life features are out of scope. |

Use the VanillaBTN name for the native product and its documentation. The maintainer
can decide separately whether to rename the GitHub repository.

## Verified findings

- **Windows tooling is installed.** Visual Studio includes MSVC x64 19.52,
  CMake 4.3.1, and Ninja 1.13.2. Use the x64 developer shell for native builds.
  The `upstream` remote already points to `coalslav/hmbtn-decomp`.
- **PsyZ has module support and an XA decoder.** At revision `00fb42e`, CMake
  exposes `psyz_add_module` and `psyz_exports`, and offers `sdl3_gpu` and `sdl3_gl`.
  The SDL submodule points to the 3.4.20 release commit.
  [PsyZ CMake](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/CMakeLists.txt) ·
  [Module support](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/cmake/module.cmake) ·
  [CD and XA implementation](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/psyz/libcd.c) ·
  [SDL 3.4.20](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20)
- **Disc import and runtime reads.** `tools/psx_disc.py import` accepts BIN or CUE and
  verifies the USA reference before extraction. `tools/afile.py unpack` unpacks A_FILE.
  The Windows startup executable invokes the importer with `--import-disc`.
  Interactive disc selection and in-game reads through PsyZ remain separate milestones.
- **PsyZ licensing needs clarification before public integration.** VanillaBTN
  uses MIT, and `NOTICE` identifies the upstream base as CC0. PsyZ marks
  `psyz/include` and `psyz/src/psyq` as *Unlicensed*. A submodule pins a revision
  and leaves its licensing unchanged. Confirm the terms for files used by the build
  before public integration or binary distribution, and record them in `NOTICE`.
  [PsyZ license](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/LICENSE)

## Porting risks

| Area | What to verify |
|---|---|
| Overlays | Measure initial state, reload behavior, and transitions across both PS1 load regions. Try one Windows module with host imports. Use those results and a duplicate-symbol check to choose between PsyZ modules and static linking with explicit reset. |
| Graphics and memory | Use `OT_TYPE` for ordering tables and `O_TAG` for primitive tags. Audit each `u_long` by role. Size buffers from their element types and keep host pointers separate from PS1 binary layouts. |
| Layout and addresses | Audit padding, pointer-to-integer casts, and fixed PS1 addresses in code such as `src/main/main.c` and `src/overlay/11_GF_FARM/people/family.c`. Check relevant sizes and offsets with each target's compiler. Use buffers or symbols for native addresses. |
| CPU and timing | Map GTE, scratchpad, and MIPS assembly to equivalent behavior. Reuse PsyZ interfaces where available. Compare VSync pacing and frame counts with the USA PS1 build. |
| Audio and assets | Test XA file and channel filtering, looping, stopping, and stream changes. Check SEQ/VAB music, sound effects, A_FILE reads, and texture relocation. |
| Saves | Transfer USA diaries in both directions. Save, quit, restart and compare restored state across slots and gameplay milestones. |

See the [PsyZ porting guide](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/PORTING_GUIDE.md)
for graphics types and build integration.

## Milestones and completion checks

| Milestone | Completion check |
|---|---|
| 0. Baseline | A small C program builds and runs in the Windows x64 developer shell. Local Docker verifies all 17 matching outputs with the original disc, including the 14 code binaries. |
| 1. Integration tests | A local PsyZ sample opens the USA disc, reads A_FILE, plays a game XA stream, and reloads one module. Record the tested dependency revisions, the overlay decision, and the XA results. |
| 2. Native build | Native Debug and Release presets build the executable and its overlays. The PS1 matching build still passes. |
| 3. First playable flow | Import the disc, start a new game, reach the farm, change areas, save, quit, restart, and load the save. Check graphics, audio, input, and timing along this path. |
| 4. Whole-game parity | Compare a playthrough, day and season transitions, festivals, all five minigames, the credits, repeated overlay transitions, and save compatibility with the USA PS1 reference. |
| 5. Release | Test the Windows package from a clean installation with a user-provided disc. Confirm package licenses and notices. Apply the same checks when adding Linux and macOS. |

## Local bring-up results

- **Windows x64.** MSVC, CMake, and Ninja built and ran a C smoke test in Debug
  and Release.
- **PS1.** The reference USA disc passed its input checksum. The local Docker
  build matched all 17 outputs against the reference SHA-1 values.
- **Native startup.** Both Windows x64 presets build and smoke-test the `vanillabtn`
  target. Milestone 2 also requires the game sources and overlays to build natively.
- **LLVM on Windows.** Clang builds the startup target with ASan and UBSan in
  Debug and LTO in Release. The LLVM frontend also accepts shared headers with
  GNU C extensions that MSVC rejects.
- **Windows packaging.** The Release install rules stage the startup executable,
  importer, checksum manifest, and notices. The installed startup package can
  import a user-provided disc. The playable release in milestone 5 remains pending.
- **PsyZ modules.** PsyZ `00fb42e` and its SDL 3.4.20 revision built on Windows x64.
  The upstream overlay sample passed host symbol imports, nested loading, and state
  reset on reload. Start with PsyZ modules for the native game integration and verify
  the game's own overlay transitions and symbol bindings there.
- **Disc and XA.** A local probe opened the reference disc via its CUE, checked the
  A_FILE offset table, and read an archive data sector. It decoded one filtered XA
  stream to nonzero stereo PCM. In-game playback, loops, and fades still need testing.
- **Overlay archive reads.** A Windows x64 diagnostic ran the shared A_FILE reader
  against the reference disc. It checked all 33 offsets and verified a low, high, low
  load sequence against the disc bytes, with region buffers standing in for PS1 RAM.
- **Full-tree native compile and link.** All 113 C units (38 `src/main/` and 75
  `src/overlay/`) compile with Windows x64 Clang against PsyZ `00fb42e`, with 0
  duplicate global symbols, 0 pointer-truncation warnings, and 0 buffer-bounds
  warnings. Together with `native/sdk_compat.c`, all 113 units link cleanly
  against `psyz.lib` and `SDL3-static.lib` with 0 unresolved symbols.
- **Local game build.** Set `VANILLABTN_PSYZ_SOURCE_DIR` with Windows Clang to link the same
  source manifests into `vanillabtn`. Use `--disc` to verify and mount a reference image.
  [Build and launch instructions](README.md#build-and-run-the-game-locally)
- **Windows launch smoke.** Debug with the reference BIN and an installed Release with
  the reference CUE initialized Direct3D 12 and ran for 15 seconds. CTest also verifies
  that a mismatched image is rejected before starting the game.
- **Runtime probes.** Verified A_FILE disc reads, PsyZ pad polling, 64-bit
  text-bank pointers, GTE `RotMatrixZXY` and transforms, `MoveImage2` VRAM copy,
  `libgs` lighting/offset, `kernel.h` `struct DIRENTRY`, and `libmcrd`/`libmcx`
  32 KB save/load round-trips layered over PsyZ's `libcard`/`libapi`.
- **Live multi-scene render.** All 15 scenes (`TITLE`, `FARM`, `SAVE`, `LOAD`,
  `MG1`..`MG5`, `STAFF`, `DEBUG`, `LIVESTOCK_TRADE`, `MG_DEBUG`, `MEMCARD`,
  `PDA_SAMPLE`) booted against the USA disc on Direct3D 12.
- **Milestone 3 save/load flow.** Verified `TITLE` -> Farmhouse -> Diary save
  (32,768-byte PS1 `SC` save via 32-bit `Ps1SaveImage`) -> state wipe -> `TITLE`
  reboot -> `LOAD` -> `FARM_LOADED` -> Outdoor Farm Yard with restored state.
- **DuckStation save exchange.** Verified one USA diary in each direction. On the
  return trip the native loader validated the converted export and restored its date.
  [Transfer instructions](README.md#transfer-diaries-with-duckstation)
- **Overlay re-entry.** Verified 12 repeated transitions across `TITLE`, `FARM`,
  `MG1`, `MG5`, `STAFF`, `FARM_MINIGAME_EXIT`, and `FARM_STAFF_EXIT` after fixing
  static re-entry resets, missing returns, and unprototyped calls under `__psyz`.

### Scene and event coverage

- A local Windows Debug probe requested all 21 scene IDs with the USA disc and exercised
  each route for at least 12 ticks. The PDA route resumed the farm or debug menu with no device.
- The native loader read all 255 farm sections. Their 10,397 trigger entries reference
  313 distinct IDs. All 50 cached event scripts decoded to the corresponding disc script prefix.
- An instrumented Debug run completed the new-game intro with simulated controller input.
  Dialogue rendering also passed checks for an absent livestock name and an empty string.
- An ASan/UBSan probe entered and rendered all 255 farm areas (2,165 valid exit zones),
  dispatched all 28 shops, and ran all 5 minigames plus staff, title, save/load, trade,
  memory card, and debug scenes across 2x re-entry passes.
- A strict ASan/UBSan sweep (`halt_on_error=1`) also passed across all 13 overlays,
  all 255 farm sections, all 18 festival dates, overnight rollover (`scene_overnight_step`),
  animal pickup/put-down (dog, chicken, chick, cricket, cow), and `FRAME_OT_AT` bounds.
- Shared native fixes verified under `__psyz` (PS1 still matches 17/17): widened-`AnimState`
  shadow struct alignment (`animals.c`, `swind.c`, `player.c`), stack-overflow fixes
  (`estate.c`, `player.c`), strict indirect handler prototypes (0 `-Wstrict-prototypes`
  across all 113 C units), direct OT slot clamping (`main.h`), NULL probe/request guards
  (`farm_probe.c`, `event_script.c`), and unsigned shifts (`villager.c`, `swind.c`).
- Debug builds accept `--debug-menu` with `--disc` to boot directly into `08_DEBUG`
  (rejected in Release, verified by `native/smoke.cmake`).
- All 23 populated debug menu rows passed a 280-frame ASan/UBSan run from direct boot.
  Seasonal house jumps, minigame projection and both utilities' rendering were checked.
- Of 451 event archive slots, 12 are empty. Four further slots (261 and 273–275) have an
  invalid offset range or initial opcode. The area trigger tables do not reference those four.
- Still open for manual playtesting: long multi-stage cutscenes (weddings, endings),
  interactive minigame completion/rewards, audio/timing parity, and a broader save
  exchange check across slots and game states.

### Native SDK compatibility (`native/sdk_compat.c` against PsyZ `00fb42e`)

Seven overlays (`17_GF_MG1`..`25_GF_MG5`, `27_GF_TITLE`, `28_GF_STAFF`) need no
extra SDK shims. Only `30_GF_swind` imports 15 symbols from `11_GF_FARM`. The
compatibility layer covers the remaining PsyZ `00fb42e` gaps so all 113 units link:

| Subsystem | Symbols covered | Notes |
|---|---|---|
| Kernel (`libapi`) | `SetMem` (guarded in `main.c`), `psyz_card_rename` (`31_gf_mcard`) | Translates `bu00:` paths and returns PS1 `1`/`0` status. |
| GTE (`libgte`) | `RotMatrixZXY` (`11_GF_FARM`) | Q4.12 Z-X-Y Euler matrix using PsyZ `rsin`/`rcos`. |
| GPU (`libgpu`) | `MoveImage2` (`11_GF_FARM`); `KanjiFnt*` (`08_DEBUG`, `30_GF_swind`) | `MoveImage2` forwards to `MoveImage`; `KanjiFnt*` rasterizes Shift-JIS via `native/kanji_font.c` into 4-bit VRAM. |
| Graphics (`libgs`) | `GsGetWorkBase` (mirrored in `main.c`); `GsSetOffset`, `GsInit3D`, `GsSetProjection`, `GsSetFlatLight`, `GsSetAmbient`, `GsSetLightMode` | Updates PsyZ `POSITION`, `GsSetDrawBuffOffset`, and GTE screen/light/color matrices. |
| Memory card (`libmcrd`) | `MemCardInit`, `MemCardStart`, `MemCardStop`, `MemCardEnd`, `MemCardCallback`, `MemCardAccept`, `MemCardExist`, `MemCardSync`, `MemCardGetDirentry`, `MemCardCreateFile`, `MemCardDeleteFile`, `MemCardOpen`, `MemCardClose`, `MemCardReadData`, `MemCardWriteData`, `MemCardReadFile`, `MemCardWriteFile`, `MemCardFormat`, `MemCardUnformat` | Layered over PsyZ `libcard`/`libapi` (`InitCARD`, `StartCARD`, `firstfile`/`nextfile`, `psyz_open/read/write/lseek/close`, `erase`) with path and alignment validation. |
| PocketStation (`libmcx`) | `McxStartCom`, `McxStopCom`, `McxCardType`, `McxSync`, plus 15 sample-only calls in `10_PdaSamp` | Reports PocketStation absent (`McxErrInvalid` on valid card ports) so regular memory-card probing succeeds. |

All 113 units have 0 pointer-to-integer, buffer-bounds, `-Wstrict-prototypes`,
and `-Wreturn-type` warnings. Verify audio, day and season parity, and cross-check
saves across more game states and a physical PS1 if available.

## Verification

- Follow [the project rules](AGENTS.md). Run `tools/docker_build.sh` locally for
  changes that affect the matching build. Check 14 code binaries, or all 17 files
  with the original disc. GitHub Actions stays manual.
- Use separate PS1 and native build directories. Limit `__psyz` conditionals to
  platform-specific behavior. Change shared types and layouts in small steps.
- Smoke-test native changes before and after once the executable is available.
  Follow the Debug and Release requirements in `AGENTS.md`, including path redaction
  in logs. Validate asset and save input in both configurations.
- Test first-run import with a valid disc, a wrong-region or damaged image, and
  a retry after failure. Keep imported data and saves in separate directories.
