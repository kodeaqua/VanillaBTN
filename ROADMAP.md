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
| Saves | Load a reference USA PS1 save into a virtual memory card. Save from native, quit, restart, and reload. Verify that the reference PS1 environment can also read the native save. |

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
- **Main native compile and link check.** All 38 `src/main/` units compile with
  Windows x64 Clang against PsyZ `00fb42e`, with runtime probes passing for A_FILE
  disc reads, PsyZ pad polling, and 64-bit text-bank pointers. PsyZ implements
  low-level `libcard`/`libapi` memory-card APIs, not `libmcrd` or `libmcx`.

### Native `src/main` link check (PsyZ `00fb42e`)

Linking all 38 `src/main/` objects against `psyz.lib` and `SDL3-static.lib` leaves
25 unresolved symbols (0 internal `src/main/` gaps):

| Subsystem | Unresolved symbols | Notes |
|---|---|---|
| Kernel (`libapi`) | `SetMem` | Declared in `libapi.h`; not built in `psyz.lib`. |
| Graphics (`libgs`) | `GsSetOffset`, `GsInit3D`, `GsGetWorkBase` | Declared in `libgs.h`; `INCLUDE_ASM` stubs in PsyZ `00fb42e`. |
| Memory card (`libmcrd`) | `MemCardInit`, `MemCardStart` | Header-only in PsyZ `00fb42e`; PsyZ implements `libcard`/`libapi` (`InitCARD`, `StartCARD`, `StopCARD`, `_bu_init`, `_card_*`, `firstfile`/`nextfile`). |
| PocketStation (`libmcx`) | `McxStartCom` | Header-only in PsyZ `00fb42e`; no implementation. |
| Overlays (`src/overlay/*`) | 15 scene handlers (`debug_tick`, `pda_sample_run`, `farm_enter_dispatch`, `save_screen_tick`, `load_screen_tick`, `save_transfer_screen_tick`, `mg1_run_state`, `mg2_phase_run`, `mg3_step_dispatch`, `mg4_run_step`, `mg5_step_run`, `title_step_dispatch`, `staff_tick`, `swind_window_tick`, `mcard_screen_tick`) and 3 farm callbacks (`farm_clamp_status`, `item_use_coin`, `farm_adjust_stamina_fatigue`) | Resolved once overlays load via PsyZ modules or host dispatch. |

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
