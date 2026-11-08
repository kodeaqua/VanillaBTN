# Work tracker

Review this file and the active branch or pull request before starting work.
Update the relevant item after completing a task or discovering a blocker.

## Active

- [ ] Connect original game code through platform/SDK interfaces. Do not rebuild scene, menu, dialog or game logic in the native host.
  - [x] Compile the original texture-pack loader against native GPU transfers.
  - [ ] Connect original primitive/ordering-table submission and animation code.
    - [x] Compile original prim.c and submit its packets through mapped native OT links.
    - [ ] Compile original animation drawing and connect frame cursor insertion.
      - [x] Compile anim.c and exercise original clock, SPRT/FT4 construction and cursor linking.
      - [x] Connect original frame-buffer layout and cursor insertion to host storage.
  - [ ] Connect original disc requests, startup and native overlay dispatch.
- [ ] Native executable remains a backend smoke harness; original game execution is not connected.
- [ ] Graphics/GTE: verify hardware rounding, saturation, flags, and primitive edge coverage against independent fixtures.
  - [ ] Verify textured-polygon subpixel interpolation with nonuniform original FT4 fixtures; packet-generation tests do not establish texel fidelity.
- [ ] Audio: implement ADSR/reverb, XA stereo histories and coding-info sample rates.
- [ ] Save/load: verify external card chain encoding, corruption handling, and original save layout before game integration.
- [ ] Developer: check controller feel and simulation pacing in native window.
- [ ] Developer: compare music, effects, speech/streams, and timing by ear.
- [ ] Developer: save, reload, and compare farms in game.
- [ ] Milestone 6: bring up farm, events, minigames, ending, title, and card flows.

## Next

- [ ] Optimization and release packaging.

## Completed

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
