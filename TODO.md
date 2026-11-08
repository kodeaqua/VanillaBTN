# Work tracker

Review this file and the active branch or pull request before starting work.
Update the relevant item after completing a task or discovering a blocker.

## Active

- [x] Developer: open and close the native Debug/Release window on Windows.
- [x] Developer: verify the importer recognizes a personally supplied USA disc.
- [ ] Milestone 2: implement required GPU primitives, CLUT, ordering, and GTE behavior with synthetic pixel checks.
  - [x] Add GP0 command dispatcher (ps1_gpu_exec) covering fill, untextured tiles, textured sprites, and draw-environment commands; Debug and Release dispatch tests pass.
  - [x] Wire OT walk to GPU dispatcher (ps1_gpu_submit_ot/frame) for frame submission; Debug and Release multi-table tests pass.
  - [x] Implement flat and textured polygons (POLY_F3/F4/FT3/FT4) for scene rendering; Debug and Release rasterization tests pass.
  - [ ] GTE compatibility layer (coordinate transform, perspective divide, clip codes).

## Next

- [ ] After graphics, map input and fixed simulation timing to the PS1 behavior.

## Completed

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
