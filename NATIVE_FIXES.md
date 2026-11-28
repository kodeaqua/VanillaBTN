# Windows native correction checklist

The bring-up build is the starting point. These are investigation and implementation
tasks. A checkbox closes on a reproducible comparison or a passing regression test.
Keep 4:3 framing, original assets, gameplay ticks, and PS1-compatible diaries throughout.

## 1. Capture a baseline

- [ ] Record a slow camera pan in the farmyard, an oblique textured surface, an actor
  passing behind scenery, and a fade into a dialogue window. Record scene, camera,
  input sequence, frame number, viewport, and build revision. Keep captures local.
- [ ] Replay each sequence on the USA PS1 reference or DuckStation's software renderer
  at native resolution with enhancements disabled. Record the emulator settings.
  Mark each symptom as observed in both, native-only, or still unconfirmed.
- [ ] In Release, capture CPU frame time, GPU frame time, present intervals, upload and
  readback bytes, and peak host memory on the same route. Record hardware and driver.
  Repeat after warm-up and compare median and 95th-percentile times.

## 2. Prioritize visible defects

| Priority and task | Where to investigate | Acceptance check |
|---|---|---|
| [ ] P1. Stabilize projected 3D vertices | PS1 projected screen coordinates are integers. Follow a vertex through the GTE calls in [model.c](src/main/geometry/model.c), packet construction, and PsyZ vertex submission. Preserve fractional screen coordinates and camera-space depth **before** packing XY for eligible 3D draws. | During the recorded pan, projected positions advance smoothly and shared edges stay joined. Check camera changes and minigame exit. HUD and sprites retain pixel alignment. |
| [ ] P1. Correct texture warping on 3D surfaces | The PS1 interpolates polygon UVs affinely. Once per-vertex depth survives projection, use perspective-correct interpolation for identified world polygons. Keep screen-space quads and sprite atlases on their existing path. | On the same sloping quad, a texture landmark stays attached during camera motion and across the triangle diagonal. Check UV boundaries, palette selection and flipped sprites. |
| [ ] P1. Diagnose popping and wrong occlusion | Trace near-camera geometry, winding, ordering-table insertion and [primitive clipping](native/gpu_primitives.c). Inspect opaque geometry separately from layered sprites, shadows, and semi-transparent draws whose result depends on submission order. | For each recorded disappearance or overlap, show which draw changed order or was culled. Verify the corrected frame and neighboring frames, including foreground scenery and effects. |
| [ ] P2. Correct dithering and color conversion | PS1 draw state can apply ordered dithering before RGB555 quantization. Compare the dither-enabled and disabled paths on a flat-to-shaded gradient. Check primitive eligibility, native pixel coordinates, and single application of quantization. | With dithering enabled, the 4×4 pattern has the reference phase and intensity. With it disabled, the same draw has no pattern. Check fades, unshaded sprites, text, and color band boundaries. |
| [ ] P2. Check blending, masks, and VRAM copies | Compare semi-transparent primitives over known background colors, palette uploads, and framebuffer copies. Follow PsyZ flushes when a draw samples a region written earlier in the frame. | Blend modes and mask-bit writes match reference pixel samples. Opening a menu or transitioning scenes shows the current texture and palette on the first visible frame. |

PGXP addresses precision loss in emulators. VanillaBTN has the original geometry
producers, so the first experiment can carry projection data directly from them.
Treat geometry precision, texture interpolation, and depth ordering as independently testable changes.

Hardware and implementation references: [GTE projection](https://psx-spx.consoledev.net/geometrytransformationenginegte/),
[GPU commands, color and blending](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/),
[DuckStation's PGXP feature description](https://github.com/stenzek/duckstation#features).

## 3. Use host hardware where measurements justify it

The game and overlays already execute as x64 code. PsyZ's SDL3 GPU backend submits
indexed draws and handles VRAM transfers. Native frame workspaces already use host RAM.
The target is correct output with stable frame times and bounded memory use.

| Resource | Task | Acceptance check |
|---|---|---|
| [ ] GPU and VRAM | Profile submissions, texture uploads, dirty-region synchronization, readbacks, and fence waits in the [pinned renderer](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_gpu.c). Keep reusable resources resident and batch compatible draws where the trace shows a cost. Preserve read-after-write dependencies and draw order. | The targeted wait or transfer decreases on the same route. Pixel probes, frame pacing, and GPU resource lifetime checks pass. |
| [ ] Host CPU | Profile game logic, packet preparation, asset decoding, and audio. If a measured decode or preparation task causes stalls, give a worker immutable input and publish its result at the existing game-thread boundary. | The stall decreases. Input, event order, game clock, and audio stay synchronized at different monitor refresh rates and during area changes. |
| [ ] Host RAM | Measure peak use and buffer occupancy in [frame buffers](src/main/graphics/frame_buffers.c), [overlay regions](src/main/overlay_regions.c), and [archive reads](src/main/cd/afile.c). Replace a limiting workspace or cache only after identifying its lifetime, owner, and size bound. Keep disc offsets, VRAM addresses, and serialized save fields in their original formats. | Repeated farm, menu, and minigame transitions reach a stable memory plateau. Allocation failure leaves saves intact. Sanitizers and diary round-trips pass. |

Use the [SDL GPU documentation](https://wiki.libsdl.org/SDL3/CategoryGPU) when choosing
host buffer, texture, transfer, and synchronization operations.

## 4. Close each fix

- [ ] Keep the smallest failing scene or synthetic fixture and record its before/after result.
  For precision corrections, document the intended visible difference from the PS1.
- [ ] Run the native Debug and Release suites and the affected gameplay route. Run the
  local PS1 matching check for changes covered by [AGENTS.md](AGENTS.md).
- [ ] Update the relevant row with evidence. Continue long-play coverage alongside development.
