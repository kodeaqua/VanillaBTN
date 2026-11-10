# Work tracker

Review this file and the active branch or pull request before starting work.
Update the relevant item after completing a task or discovering a blocker.

## Active

- [x] Continuation audit: complete farm linkage/address translation and remove unsupported SDK success paths before claiming runtime integration.
  - [x] Fix the reported flashback abort at SsSetSerialVol: the original CD fade emits 128 at volume 32, and the SDK must cap it to 127. The original-code regression fails on the old backend and after reversion; both profiles pass 32 synthetic checks. An optional Debug new-game replay reaches 18,000 vblanks beyond the previous abort.
  - [x] Investigate a separate optimized new-game probe failure in native menu state/callback handling before claiming Release gameplay stability. Guard farm menu last callback invocation when null, return early from options menu closing state to prevent out-of-bounds dispatch into options_state_handlers, and map status page sprites indices 3 and 4 explicitly to status_cursor_sprite and status_mark_sprite to handle MSVC x64 BSS non-adjacency. An asset-free Release new-game replay completes 18,000 vblanks into the farm scene with full audio/graphics without faults.

  - [x] Replace unbounded SEQ scanning with exact archive spans and reject invalid sequence headers without consuming handles.
  - [x] Finish the interrupted MSVC farm-source build: case ranges, nested helpers, byte-pointer arithmetic and required type declarations.
  - [x] Resolve startup/farm linkage, SDK bootstrap and GS/matrix entry points; preserve original frame-region addresses in host RAM.
  - [x] Preserve event-block/stash and animal/toy buffer pointers at host width; remove integer-address addition from cue/entry indexing. A real event-tail regression segfaults on the old code and passes after restoration.
  - [x] Clear estate queue kind fields through native member addresses; the original PS1 offsets overwrite x64 entryStep. Synthetic guards reproduce the corruption and protect message/timer fields.
  - [x] Translate original physical-zero scene/ground data accesses into backend RAM, retaining original null sentinels and PS1 member offsets. Fixtures cover reads and absent marked-entry writes.
  - [x] Pass helper/player receivers explicitly at native callback sites that relied on live MIPS argument registers. Off-screen helper cue fixtures fail before the fix and advance correctly afterward.
  - [x] Preserve the attract-demo field layout across its three adjacent PS1 data fragments. The native linker previously exposed unrelated memory after cell 361, producing invalid item IDs and missing farm objects. The complete 1,125-cell hash matches the code-verified PS1 overlay; regression fails before the fix and after temporary reversion.
  - [x] Read native airborne hop rows from their original movement/hop arrays instead of assuming PS1 global adjacency. The reference farm binary confirms the 51-unit row; original movement fixtures cover four directions and both hop rows, fail before the fix and after temporary reversion. Both profiles pass 30 checks, the full Release demo cycle completes, and all 14 PS1 code hashes match.
  - [x] Audit remaining farm serialized addresses/layouts and callback calling conventions; resolve wife string table pointer lookups on x64, map adjacent player probe offset tables, map status page sprite pointer layouts, and verify title overlay scratch buffer boundaries. Independent unit fixtures verify all lookups in Debug and Release (36 checks pass), the 18,000-vblank Release new-game probe passes without faults, and all 14 PS1 code hashes match.
  - [x] PocketStation operations report missing/non-PDA devices without fabricated successful reads or writes; restore canonical SDK result/command values.
  - [x] Make SDK card completions consumable, preserve request-versus-result semantics and cap directory output to its supplied capacity.
  - [x] Reject invalid/truncated memory-card transfers and validate file chains before touching caller buffers or saves; 29 synthetic checks pass in both profiles. SDK request/completion semantics are verified separately; open/data APIs explicitly reject unsupported requests.
- [x] Connect original game code through platform/SDK interfaces. Do not rebuild scene, menu, dialog or game logic in the native host.
  - [x] Compile the original texture-pack loader against native GPU transfers.
  - [x] Connect original primitive/ordering-table submission and animation code.
    - [x] Compile original prim.c and submit its packets through mapped native OT links.
    - [x] Compile original animation drawing and connect frame cursor insertion.
      - [x] Compile anim.c and exercise original clock, SPRT/FT4 construction and cursor linking.
      - [x] Connect original frame-buffer layout, system display-mask helpers, loading screen, UI icons, background tiling, camera setup, 3D model/ground geometry (`model.c`, `ground.c`, `vec_length.c`), CLUT fades/scrolls, math_util.c and cursor insertion to host storage.
  - [x] Connect original disc requests, startup and native overlay dispatch.
    - [x] Provide SDK VSync timing with deterministic synthetic clock checks.
    - [x] Compile original afile.c, cd_audio.c, sound.c, sound_request.c and event_flags.c against imported-disc SDK sector reads, XA control commands and libsnd/libspu bridges.
    - [x] Map original overlay data regions into bounded host main RAM.
    - [x] Honor SDK-selected draw/display rectangles and display mask in SDL presentation.
    - [x] Compile and exercise original screen fades, `farm_state.c`, `farm_tables.c` and `11_GF_FARM` shared tables/vars (`farm_data.c`, `farm_vars.c`) through `game_screen`.
    - [x] Compile original window-frame drawing, text engine (`text.c`), menu window (`menu_window.c`), heart icon (`heart.c`), name entry (`name_entry.c`), `11_GF_FARM` picture/storage helpers (`picture_cells.c`, `menu/item_storage.c`), `08_DEBUG/debug.c`, PDA sample screen (`10_PdaSamp/pdasamp.c`), all 5 minigames (`17_GF_MG1`..`25_GF_MG5`), title overlay (`27_GF_TITLE/title.c`), staff-roll overlay (`28_GF_STAFF/staff.c`), and memory-card test screen (`31_gf_mcard/mcard.c`), and support `DR_AREA` (`SetDrawArea`), `SetDrawMode`, `SetPolyF4`, `SetSemiTrans`, `AddPrim`, `MoveImage2`, GTE matrix transforms (`RotMatrix`, `SetRotMatrix`, `SetTransMatrix`, `RotTrans`), SPU channel masks, line/Gouraud packets, GPU font helpers (`FntLoad`, `FntOpen`, `FntFlush`, `SetDumpFnt`), and PS1 memory-card / PocketStation SDK bridges (`libmcrd`, `libmcx`).
    - [x] Compile original startup/main loop and save/load overlay with MSVC; SDK declarations checked against the local reference headers.
    - [x] Dispatch original VSync callbacks and provide a cooperative host pump, with per-vblank synthetic timing checks.
    - [x] Connect original startup and module dispatch to the host loop; both profiles boot the original title through a finite headless probe (901 vblank pumps, 848 colored frames, 658 nonzero PCM intervals). No scene logic is replaced.
- [x] Replace the blank backend harness with original startup/game execution; full-game acceptance remains open.
- [x] Connect original controller polling through native SDK receive buffers and rumble.
  - [x] Restore both gamepads' held buttons, stick and trigger states when window focus returns. An SDL virtual-gamepad fixture verifies snapshot reads and releases; physical focus behavior remains developer validation.
  - [x] Track keyboard aliases per physical key, so releasing one alias does not release a PS1 button still held by another. Synthetic input checks cover overlapping aliases.
  - [x] Map both SDL gamepad trigger axes to digital L2/R2 for both ports. Synthetic boundary and combined-trigger checks pass; the developer must validate physical controller behavior.
  - [x] Match SDK atan/CORDIC math across all 16,641 analog magnitude pairs in both profiles.
  - [x] Compile original pad.c and pad_vibration.c against native receive buffers; verify repeats, sectors, replay, hot swaps and vibration script actuator requests.
  - [x] Invoke original pad polling from original startup/frame execution.
  - [x] Assign a second physical host controller; the SDL host tracks dual opened
    gamepads and routes port 2 buttons, stick axes, hot-swap disconnects and rumble.
    Synthetic fixtures cover dual-port analog sampling and actuator state in Debug
    and Release (31 checks pass), and all 14 PS1 code hashes match.
- [x] Graphics/GTE: verify hardware rounding, saturation, flags, and primitive edge coverage against independent fixtures.
  - [x] Match staged fixed-point products in RotMatrixZXY against local reference SDK numerical outputs.
  - [x] Saturate SZ to 16 bits, preserve triple-projection FIFO order and use SDK ZSF3/ZSF4 scales for average depth; long demo reaches 5,000 vblanks without invalid GPU links.
  - [x] Correct NCLIP winding and MAC0 overflow flags. Independent signed-area and original TMD front/back tests reproduce the inverted culling; a captured native farm frame now contains terrain. Colored-frame counts alone are not a geometry acceptance test.
  - [x] Preserve translation in SDK matrix snapshots and distinguish saturated IR stores from wide MAC stores; register loads truncate like hardware. Two regressions fail before the fix; Debug/Release pass 30 checks and all 14 PS1 code hashes match.
  - [x] Saturate projected SXY coordinates to the hardware signed 11-bit range and set axis overflow flags; RTPS/RTPT regressions fail before the fix and after temporary reversion. Both native profiles pass 30 tests and the 14-file PS1 code check passes.
  - [x] Replace ideal projection division with the GTE UNR reciprocal, near-plane overflow, signed fixed-point shifts, IR saturation and the hardware summary-bit mask. Independent numerical fixtures fail before the fix and after temporary reversion; both native profiles pass 30 checks, the Release demo cycle completes 10,000 vblanks, and all 14 PS1 code hashes match. Full MAC overflow and depth-cue behavior remain open.
  - [x] Replace csin/ccos aliases with the distinct SDK function quantization.
    Independent numerical fixtures verify csincos, csin, and ccos across all four
    quadrants in Debug and Release (31 tests pass), and all 14 PS1 code hashes match.
  - [x] Interpolate all vertex colors for textured Gouraud GT3/GT4 packets before texel modulation. Independent nonuniform white-texture fixtures fail on the old backend; Debug/Release pass 30 checks and the 5,000-vblank Release demo passes GPU validation. Full object visibility and placement remain under audit.
  - [x] Verify textured-polygon subpixel interpolation with nonuniform original FT4 fixtures. Independent nonuniform fixtures verify standard forward FT4, oriented-UV FT4, and SPRT parts matching Sony libgpu polygon affine truncation; both native profiles pass 31 checks and all 14 PS1 code hashes match.
  - [x] Restore the GS projection center after scene resets. In the supported GS mode, the SDK adds the inactive buffer's VRAM origin to the requested offset or persistent center; original startup defines both origins as (0, 0). Reference-code inspection and synthetic projection fixtures cover nonzero buffer origins and offset restoration. GS buffer swapping and other graph modes remain unsupported.
  - [x] Move GTE initialization from `GsInit3D` to `GsInitGraph` as in the reference SDK. Synthetic camera projection checks fail with the old reset and after its temporary restoration; graph initialization also resets a modified rotation matrix.
- [x] Audio: audit hardware envelope and reverb fidelity before claiming completion.
  - [x] Correct XA canonical parameter offsets, signed predictor rounding and reserved shifts; decode both 4-bit and 8-bit units with separate stereo histories. Independent numerical fixtures expose the old decoder and verify sample counts and invalid-width rejection in Debug/Release.
  - [x] Replace approximate ADSR rates with shift/step clock accumulators, exponential modes, exact sustain targets and release semantics. Numerical per-clock fixtures cover fast, slow and frozen rates; they fail on the old backend and after temporary reversion, and pass in Debug/Release. Full hardware sound fidelity remains open. The archive/SDK fixture now supplies real ADSR and waits for release before voice reuse. A clean committed-source snapshot plus this fixture passes all 31 tests in both profiles; all 14 PS1 code hashes match. Concurrent uncommitted farm edits are excluded.
  - [x] Remove the fabricated global two-tap echo while retaining SDK reverb controls. A dry-versus-configured PCM regression fails on the old mixer and after temporary reversion; seven related checks pass in Debug/Release.
  - [x] Implement reverb RAM addressing/wrapping, IIR same/cross reflections, four comb taps, two all-pass stages, alternating half-rate channels and 39-tap input/output FIRs. Native SDK modes 0..9 use reference-verified registers/work areas; VAB mode 4/EON and CDREV select sends. Depth/RVol share EVOL, disabling writes preserves RAM output, and echo/delay controls update their hardware registers. Independent arithmetic, clipping, FIR, wrap/guard, routing, tail and block-boundary fixtures pass all 32 checks in Debug/Release; removing the DSP makes the tail regression fail again.
  - [x] Connect original CD filter/seek/ReadS controls to bounded file-backed XA decoding and the SDK/SDL mixer. Preserve pause/resume buffers, cancel on data reads/unmount, report the 150-sector/s playback clock, and use 2336-byte imported sector strides with logical 2048-byte file sizes. Match serial-A CD volume scaling and mix gating to the reference SDK; synthetic PCM/filter/seek/mute tests cover the actual SDK path in both profiles.
  - [x] Retain the final decoded XA frame through lookahead EOF and use exact 44.1 kHz clock ratios. All eight mono/stereo, 4/8-bit and sample-rate combinations have independent exact-duration fixtures that fail before the fix and after temporary reversion; both profiles pass all 31 checks.
  - [x] Replace linear XA interpolation with the documented seven-phase zigzag FIR, persistent 32-sample stereo history, per-product truncation/saturation and 18.9 kHz input duplication. Independent impulse values fail on the linear backend and after temporary reversion; both profiles pass all 31 checks. The published hardware model has known small rounding uncertainty and deterministic startup phase.
  - [x] Gate XA sector availability at the C8 drive rate (one sector per 294 output frames), distinguish temporary underrun from EOF, preserve FIR state across gaps, and separate Setloc targets from the active read head. Fixtures verify interleave gaps, pause/resume, seek dispatch, mute progression and polling without clock advancement; all 31 checks pass in both profiles. Mechanical optical seek/spin-up latency is not synthesized from an arbitrary distance formula.
- [x] Save/load: verify original save layout before game integration.
  - [x] Persist successful SDK file writes before reporting completion, rather than waiting for host shutdown. Failed writes leave the prior card state intact; a subsequent metadata operation cannot accidentally commit the failed write. Synthetic fixtures cover immediate disk contents and error handling.
  - [x] Persist SDK format, create, delete and rename before reporting success. Stage disk-backed mutations so failed writes leave both in-memory and disk images unchanged; synthetic checks cover each success and failure path.
  - [ ] Implement persistent unformat if the memory-card test screen needs it; disk-backed requests currently fail without mutating the card.
  - [x] Preserve the existing backup when the primary card image cannot be moved during saving. A Windows sharing-lock fixture reproduces the lost backup and verifies both images remain intact.
  - [x] Preserve unresolved temporary and backup card images when the primary is missing. Synthetic Windows sharing-lock fixtures cover both candidates, and exclusive temporary creation prevents direct saves from overwriting a pending candidate.
  - [x] Preserve the primary card image if recovery cannot rename a validated temporary or backup image. A Windows sharing-lock fixture reproduces the deletion in the old code and verifies both images survive the failed recovery.
  - [x] Reject formatting a missing virtual card without changing its retained bytes or presence; synthetic SDK checks cover missing media and successful formatting of present media.
  - [x] Return 1 from `McxCardType` to accept PocketStation card-type queries; returning 0 caused original `save_card_probe` to fail with `SAVE_CARD_ERROR` and freeze at `SAVE_STEP_PROBE_AND_CHECK_SPACE`. Initialize `MemCardSync` output pointers even on idle syncs to prevent reading stack garbage.
  - [x] Expose an explicit --card-dir for isolated gameplay-test cards. Reject missing/oversized directory selections before loading; retain default preference images. A reported older card has legacy one-based links and no valid game header, while the second card is empty; do not treat directory repair as farm-state recovery.

  - [x] Give the original save header, icon frames, slot record and payload one aligned native backing image. The original reset, shift/checksum and 4-block SDK transfer are exercised by an asset-free regression that fails before the fix and after temporary reversion; both profiles pass 31 checks and all 14 PS1 code hashes match.
  - [x] Translate fixed PS1 SaveFile/SaveImage and state pack/unpack layouts to x64 structures, including serialized pointer fields and expanded animal/player/helper records. Exact PS1 wire structs with compile-time assertions and state pack/unpack translations preserve 32-bit save compatibility on x64; both native profiles pass 31 checks and all 14 PS1 code hashes match.
  - [x] Preserve failed card loads instead of autoformatting existing media; reject short/oversized images, validate recovery directory checksums, report creation/flush failures and retain dirty state after failed persistence. Synthetic file regressions fail before the fix and after temporary reversion; Debug/Release pass 30 checks and all 14 PS1 code hashes match.
  - [x] Use zero-based PS1 wire links with shared validated physical-block resolution in core and SDK reads/writes. Reject cyclic, truncated, cross-linked and orphaned chains before mutation; reuse deleted blocks, round file allocations, initialize broken-sector sentinels and the write-test frame. Independent raw-directory regressions fail before the fix and after reversion; both profiles pass 30 checks and all 14 PS1 code hashes match.
- [x] Trace the reported large attract-demo displacement to the original `scene_enter` area transition, rather than a large velocity; temporary write-watch instrumentation was removed. This does not establish console-equivalent demo behavior.
- [ ] Developer: check controller feel and simulation pacing in native window.
- [ ] Developer: compare music, effects, speech/streams, and timing by ear.
- [x] Developer: confirm save and load work in game.
- [x] Developer: confirm a moved rock retains its position in the tested farm state.
- [ ] Developer: validate other farm-state changes across scenes, days and save/load.
- [ ] Milestone 6: bring up farm, events, minigames, ending, title, and card flows.

## Next

- [ ] Optimization and release packaging.

## Completed

- [x] Add an optional `--cycle` original-code probe requiring farm input replay and return to title. Release completes 10,000 vblanks with valid GPU submission; both build profiles pass 30 asset-free checks.

- [x] Extend the optional original-code boot probe to 5,000 vblanks with a framebuffer capture and test-only crash symbols; keep cards in memory and diagnostics outside the game executable.

- [x] Allow a simple named Windows runtime candidate through the build wrapper while another executable is in use; default and candidate Debug builds and the Release build pass 30 tests.

- [x] Remove fabricated PocketStation serial/time/memory and successful execution/write responses. Standard cards report Invalid (2), missing cards NoCard (1), with canonical command IDs and untouched buffers/card contents. Regression fails on the old bridge and on temporary reversion; both profiles pass 28 tests. No PocketStation device is emulated.
- [x] Fix SDK sequence opening: carry original archive byte counts into one-shot SDK source views, remove raw EOT byte scanning, and check parser failure before publishing a handle. Invalid PPQN fails before/after temporary reversion; short buffers and an FF/2F delta are covered. Both native profiles pass 27 checks, and the 14-file PS1 code check passes.
- [x] Compile original PDA sample screen `10_PdaSamp/pdasamp.c` in `game_window_frame`, and add PocketStation `libmcx` SDK bridges (`McxAllInfo`, `McxGetApl`, `McxGetTime`, `McxHideTrans`, `McxGetSerial`, `McxGetMem`, `McxExecFlag`, `McxExecApl`, `McxCurrCtrl`, `McxFlashAcs`, `McxSetMem`, `McxShowTrans`, `McxSetLED`, `McxReadDev`, `McxWriteDev`, `McxSync`). Synthetic checks exercise `pda_sample_run` loop execution, pad polling delivering `PAD_SELECT` through native SDK buffers, and exit scene request to `SCENE_DEBUG` in Debug/Release (27 checks), and all 14 PS1 code hashes match.

- [x] Compile original memory-card test screen `31_gf_mcard/mcard.c` unchanged in `game_window_frame`, and add PS1 memory card SDK bridges `libmcrd` and `libmcx` backed by `ps1_card` with `sdk_card_rename`, `MemCardAccept`, `MemCardSync`, `MemCardFormat`, `MemCardUnformat`, `MemCardGetDirentry`, and GPU font streams (`FntLoad`, `FntOpen`, `FntFlush`, `SetDumpFnt`). Synthetic checks verify menu layout, card probe type (`MCARD_CARD`), rename of save files (`BISLPS-00000-FARM0` -> `BISLPS-02489-FARM0`), directory listing, unformat/format cycle, and reboot debug scene request in Debug/Release (27 checks), and all 14 PS1 code hashes match.

- [x] Compile original `08_DEBUG/debug.c` and minigames `17_GF_MG1/mg1.c`, `19_GF_MG2/mg2.c`, `21_GF_MG3/mg3.c`, `23_GF_MG4/mg4.c`, and `25_GF_MG5/mg5.c` in `game_window_frame`, adding `KanjiFnt*` debug streams, GTE `RotMatrix`/`SetRotMatrix`/`SetTransMatrix`/`RotTrans`/`csin`/`ccos`, `SPU_00CH`..`SPU_23CH` masks, and guarding nested `mg2_draw_board_marker` under `VANILLABTN_NATIVE`. Synthetic checks verify `debug_tick` setup/menu scene request, `mg1_run_state` fade-out, `mg2_player_set_action`/`mg2_move_step`, and `mg4_run_step` phase transition and farm exit in Debug/Release (27 checks), and all 14 PS1 code hashes match.
- [x] Compile original `27_GF_TITLE/title.c` and `28_GF_STAFF/staff.c` overlays unchanged in `game_window_frame`, and add `SetDrawMode`, `setPolyF4`/`SetPolyF4`, `SetSemiTrans` and MSVC `__builtin_memcpy` compatibility to `native/compat` and `sdk_gpu`. Synthetic checks verify `title_step_dispatch` new-game farm/player/options/toolbox initialization and continue-load scene transition, and `staff_tick` entry and route-dependent exit scene requests (`SCENE_FARM_STAFF_EXIT` vs `SCENE_TITLE`) in Debug/Release (27 checks).
- [x] Compile original `farm_state.c`, `farm_tables.c`, `11_GF_FARM/farm_data.c` and `11_GF_FARM/farm_vars.c` in `game_screen`, and `11_GF_FARM/picture_cells.c` and `11_GF_FARM/menu/item_storage.c` in `game_window_frame`, preserving 64-bit host pointers in `main_get_region_base` and `item_villager_reaction` under `VANILLABTN_NATIVE` and adding `AddPrim`, `setRGB0`, `setXYWH` and `MoveImage2` to `libgpu.h`. Synthetic checks verify `farm_state_init`, `farm_tables_register`, `farm_work_sector_get`/`release`, item prices/seeds/crops, `item_villager_reaction`, `farm_clamp_status`, `farm_adjust_stamina_fatigue`, `item_use_coin`, `picture_grid_clear`, `picture_slot_claim`, `picture_release_item`, `item_picture_slots_clear` and `item_picture_slots_unmark` in Debug/Release (27 checks), and all 14 PS1 code hashes match.
- [x] Compile original `model.c`, `ground.c` and `vec_length.c` in `game_frame_buffers`, connect GTE coprocessor macros (`inline_c.h`, `gtemac.h`, `libgte.h`) and matrix/vector operations (`SetGeomOffset`, `SetGeomScreen`, `PushMatrix`, `PopMatrix`, `RotMatrixX/Y/Z`, `ApplyMatrixLV`, `OuterProduct0`, `gte_OuterProduct12`, `VectorNormal/S/SS`, `SquareRoot0`, `GsIDMATRIX`, and `gte_*` load/exec/store calls) in `sdk_math` to `ps1_gte`, and add `POLY_F3/FT3/G3/GT3/G4/GT4` (`GP0 0x34..0x3F`) support. Synthetic checks verify `model_bind_tmd`, `ground_enable_objects`, `ground_find_object`, `ground_height_at`, `ground_hit_primitive`, `camera_init_projection`, `model_load_lighting`, `gte_project_vertex`, and `model_draw` in Debug/Release (27 checks), and all 14 PS1 code hashes match.
- [x] Compile original `text.c`, `menu_window.c`, `heart.c` and `name_entry.c` in `game_window_frame`, add `DR_AREA`/`SetDrawArea`, `HWD0`/`VWD0` and `SystemError` to the native SDK boundary, and preserve 64-bit pointers in `LOCALIZED`/`LOCALIZED_NAME` (`text_bank.h`). Synthetic checks verify `text_format_number`, `text_write_season`, `text_write_weekday`, `text_list_append`, `text_match_either`, `text_window_draw` typing/close lifecycle with `DR_AREA` clipping, `menu_box_animate`, `heart_icon_draw`, and `name_entry_start`/`name_entry_step` in Debug/Release (27 checks), and all 14 PS1 code hashes match.
- [x] Connect `sdk_sound` (`libsnd`/`libspu`) to `ps1_spu` and `ps1_seq` and compile original `sound.c` and `sound_request.c` in `game_afile`. Synthetic checks verify `sound_fader_init`, `sound_init`, `sound_init_stereo_xa`, master-volume fader steps, `sound_request(SOUND_REQUEST_RESET)` VAB chunk streaming via `sound_load_tick`, `sound_play` ADPCM voice playback and `sound_queue_seq_pair`/`sound_seq_start_reverb` sequence playback in Debug/Release (27 checks), and all 14 PS1 code hashes match.
- [x] Compile original `camera.c`, `ui_icons.c` and `bg_tile.c` in `game_frame_buffers`, and `shop_restock.c`, `cooking_recipes.c`, `text_bank.c`, `picture_codes.c`, `horse_names.c`, `farm_data_size.c` and `debug_stubs.c` in `game_screen`. Synthetic checks verify `camera_init`, UI icon blink cycles/sprite drawing, 165-tile background tiling, recipe/picture/glyph tables, and distinct horse-race runner restocking in Debug/Release (27 checks).
- [x] Compile original `cd_audio.c` and `event_flags.c` in `game_afile` with `sdk_cd` XA control commands (`CdlStandby`, `CdlSetfilter`, `CdlSetmode` `0xC8`, `CdlGetlocP`, `CdlReadS`, `CdControlB`, `CdControlF`, `CdLastCom`) and `sdk_sound` (`SsSetSerialVol`). Synthetic checks exercise `xa_search_files`, `cd_audio_play_track` (valid, out-of-range and event-flag muted), `cd_audio_fade_tick` (fade-out, end-of-track stop and loop seek), `cd_audio_stop`, `event_flag_set`/`event_flag_get`, and `afile_read_bytes` stopping active XA playback in Debug/Release (27 checks).
- [x] Support inline `DR_LOAD` (`GP0 0xA0`) CPU-to-VRAM packets, add `rsin`/`rcos` to `sdk_math`, and compile original `clut.c` and `math_util.c` in `game_frame_buffers`. Synthetic checks verify odd-width/out-of-bounds `GP0 0xA0` transfers, `StoreImage` palette capture, `clut_fade_step` midpoint blending with STP bit rules, `clut_scroll_step` bounce/wrap modes, and polar vector math in Debug/Release (27 checks).
- [x] Compile original system.c (`host_poll`, `display_on`, `display_off`) and loading_screen.c in game_anim/game_frame_buffers without changing their source. Synthetic checks exercise loading-picture texture upload, display-mask toggling, double-buffered frame/environment flips, SPRT/DR_MODE submission at (160, 120) and stop transitions in Debug/Release (27 checks).
- [x] Compile original pad_vibration.c alongside pad.c in game_pad. Synthetic checks exercise vibration pattern startup gating before pad_init, script step durations, same-script deduplication, actuator0/actuator1 updates through sdk_pad rumble callbacks, explicit stop and replay suppression in Debug/Release (27 checks).
- [x] Connect host raw input/rumble adapters and compile original pad.c without replacing its logic. Synthetic fixtures exercise two SDK ports, original repeat timing, analog dead zone/d-pad priority, exact sector boundary, replay button swap, actuator negotiation and analog/digital hot swaps. Both profiles pass 27 checks and all 14 PS1 code hashes match; gameplay/controller feel remains developer validation.
- [x] Implement native SDK csqrt/ratan2 with the reference SDK's fixed-point quantization. Optional VM microchecks execute only SDK math functions from the local reference ELF, with an explicit leading-sign-bit hook, and compare all analog magnitudes against Debug/Release. Normal builds require neither the VM package nor SDK files; 26 synthetic checks pass.
- [x] Correct missing original window borders/gradients by supporting flat-line and untextured Gouraud polygon commands in the backend. The unchanged window_frame.c fixture fails before the fix; separate line/Gouraud removal makes it fail again. Both profiles pass 25 checks; all 14 PS1 code hashes match. Hardware dithering and diagonal precision remain open.
- [x] Compile original screen_fade.c and lazily map its private packets through the native SDK. Synthetic frames verify original fade counters and VRAM subtraction in/out; Debug/Release pass 24 checks. Main-loop frame globals/control hooks are fixture boundaries, not game startup.
- [x] Add SDK draw/display environments, clear and VRAM moves; SDL follows selected frame-buffer location and original internal size. Memory-only software-renderer fixtures verify buffer flips, display mask and 320x240/640x480 texture changes without opening a window. Debug/Release pass 23 checks.
- [x] Map 2 MB main RAM and its cached/uncached aliases without pointer truncation, and compile the original overlay-region table with host-backed data offsets. Boundary/overflow/alias/reset tests pass in both profiles (22 checks). Original startup and native scene dispatch remain pending; imported MIPS bytes are never executed.
- [x] Read synthetic disc trees through original A_FILE initialization, blocking/range and start/poll reads, using native CD SDK entry points. Validate HDT size/offsets before scratch reads, preserve sector padding, cancel pending reads and reject unsupported raw/XA modes. Debug/Release pass 21 checks; XA control is not connected yet.
- [x] Add SDK VSync(0/1/n/-n) timing on an SDL monotonic clock, with nominal 60 Hz vblank/15,750 Hz horizontal units and no extra wait when work is late. Debug/Release pass 20 checks. Interrupt callback registration and hardware timing remain open.
- [x] Compile original frame_buffers.c with host-safe OT insertion. Synthetic checks exercise both original buffer layouts, clearing, cursor advancement, depth clamping and final VRAM output in Debug/Release (19 checks).
- [x] Compile original animation routines against native packet/link APIs. Synthetic fixtures cover section binding, duration/hold/pause, SPRT/FT4 upload-to-draw paths and original UV orientation fields; Debug/Release pass 18 tests.
- [x] Run original primitive builders through native SDK packet submission, reverse OT traversal and bounded GS work storage. Synthetic fixtures verify color bytes, layer order, same-bucket order and rejection of unmapped/cyclic/oversized links in Debug/Release.
- [x] Add native LoadImage/StoreImage/DrawSync entry points and compile original texture_pack.c with preserved file offsets. Synthetic fixtures exercise raw/RLE uploads through the original loader; the host binds its VRAM to the SDK bridge. Full game execution remains pending.
- [x] Decode GP0 packet color bytes in PS1 order; an independent raw-byte red fixture protects the backend conversion.

- [x] Keep SEQ note/control/termination and allocation from modifying active voices owned by other banks or raw effects. The current mixer supports one sequence; shared-bank ownership and SDK voice reservation remain open.

- [x] Use nominal 30 Hz two-vblank game cadence, 44.1 kHz audio and FAST_2 input-repeat scaling in the backend harness.

- [x] Mix SEQ through the shared SPU/SDL audio backend and open/close gamepads with separate keyboard/controller state. Synthetic tests cover backend behavior; no native game scene is implemented.

- [x] Decode native texture-bank RLE/raw blocks and offset-based ANI sprites/quads with UV orientation; synthetic pixel/bounds checks pass in both profiles.
- [x] Add sample-timed SEQ note/program/volume/pan events, running status, tempo, and loop scheduling; synthetic timing and malformed-loop checks pass in Debug/Release.
- [x] Map signed host sticks through the original pad dead zone and four cardinal sectors; synthetic axis checks pass in both profiles.
- [x] Exclude polygon bottom/right edges and give shared edges one owner; translucent quad regression passes in Debug and Release.
- [x] Correct VAB header fields/sparse programs, ADPCM shifts, interpolation overflow, and final-block playback; synthetic checks pass in Debug and Release.
- [x] Milestone 5 foundation: standalone virtual card API; original format/UI compatibility remains under audit.
  - [x] Emulate dual 128 KB PS1 memory card images, Frame 0 header, and directory allocation chains (ps1_card); Debug and Release tests pass.
  - [x] Implement 32 KB HM:BTN farm save slots (BASLUS-01115-FARM0..2) with 0x200 header checksum validation; Debug and Release tests pass.
  - [x] Add safe atomic file writing with .tmp staging, .bak backup, and crash recovery; Debug and Release recovery tests pass.
  - [x] Wire Ps1CardManager with SDL pref path into main.c; Debug and Release storage tests pass.
- [x] Milestone 4 foundation: standalone sample decode/mixing and SDL PCM output; music/game integration remains pending.
  - [x] Emulate PS1 24-voice SPU mixer and VAB/ADPCM decoding (ps1_spu); Debug and Release decode tests pass.
  - [x] Implement CD-ROM XA sector stream filtering, ADPCM decoding, and track fading (ps1_xa); Debug and Release XA tests pass.
  - [x] Bridge SPU and CD-XA PCM output to SDL3 AudioStream with headless fallback (ps1_audio), wired into main loop; Debug and Release playback tests pass.
- [x] Add fixed 60 Hz simulation clock (ps1_clock) and wire keyboard/gamepad polling into native window; Debug and Release clock tests pass.
- [x] Emulate PS1 InputRecord button states and auto-repeat channels (ps1_input); Debug and Release repeat tests pass.
- [x] Milestone 3: map input and fixed simulation timing to the PS1 behavior.
- [x] Developer: open and close the native Debug/Release window on Windows.
- [x] Developer: verify the importer recognizes a personally supplied USA disc.
- [x] Milestone 2 foundation: a GPU/GTE subset with synthetic checks; hardware fidelity remains pending.
  - [x] Add GP0 command dispatcher (ps1_gpu_exec) covering fill, untextured tiles, textured sprites, and draw-environment commands; Debug and Release dispatch tests pass.
  - [x] Wire OT walk to GPU dispatcher (ps1_gpu_submit_ot/frame) for frame submission; Debug and Release multi-table tests pass.
  - [x] Implement flat and textured polygons (POLY_F3/F4/FT3/FT4) for scene rendering; Debug and Release rasterization tests pass.
  - [x] Add host-side GTE compatibility layer (ps1_gte) for transforms, projection, culling, and depth; Debug and Release math tests pass.
- [x] Implement flat and textured polygons (POLY_F3/F4/FT3/FT4) for scene rendering; Debug and Release rasterization tests pass.
- [x] Wire OT walk to GPU dispatcher (ps1_gpu_submit_ot/frame) for frame submission; Debug and Release multi-table tests pass.
- [x] Add GP0 command dispatcher (ps1_gpu_exec) covering fill, untextured tiles, textured sprites, and draw-environment commands; Debug and Release dispatch tests pass.
- [x] Add host-safe PS1 ordering-table links and reverse bucket traversal; Debug and Release ordering tests pass.
- [x] Draw raw and modulated PS1 sprites with CLUT transparency, clipping, and mask tests in Debug and Release.
- [x] Decode raw PS1 4-bit, 8-bit, and 15-bit TPage/CLUT texels; Debug and Release synthetic checks pass.
- [x] Add software quick fill and untextured tile rules with clipping, mask, and blend tests in Debug and Release.
- [x] Add native 16-bit VRAM storage and SDL presentation at original 320x240 pixels; Debug and Release pixel checks pass.
- [x] Map PS1 platform boundaries and add a Windows disc importer; synthetic checks and all 17 PS1 reference hashes pass.
- [x] Merge PR #1, sync `main`, and delete its work branch.
- [x] Add the native Windows MSVC/CMake/SDL3 build foundation; Debug and Release pass headless CTest, and all 14 PS1 code hashes still match.
- [x] Apply MIT to the project's current work and retain the earlier CC0 grant in the license notes.
- [x] Define the native Windows bringup roadmap and first port stack in `docs/roadmap.md`; native implementation remains next.
- [x] Add agent planning, synthetic testing, and developer gameplay-test rules; the code-level version test passed.
