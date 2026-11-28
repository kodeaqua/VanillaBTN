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

## 4. Work queue by subsystem

Start with the farmyard camera pan. Track one fence corner and one ground texture
landmark at each simulation tick. This separates camera motion, vertex rounding,
texture interpolation, and uneven presentation before changing any of them.

### Projection, camera, and visibility

Start in [camera.c](src/main/geometry/camera.c), [model.c](src/main/geometry/model.c),
[ground.c](src/main/geometry/ground.c), and the [scene view](src/overlay/11_GF_FARM/world/scene_view.c).

- [ ] **Locate the first rounding step.** Trace one model vertex and one terrain vertex
  through world, camera, projected, packed, and submitted coordinates. Record where
  fractional motion first disappears on each path.
- [ ] **Keep vertex identity across primitives.** Draw two adjoining faces sharing a
  corner, then two distinct corners that round to the same XY at different depths.
  The shared corner must stay joined and the distinct corners must keep their own depth.
- [ ] **Keep precision data within its frame.** Reuse a packet address after an area
  transition and alternate frame buffers. Verify that the new draw receives the new
  vertices and that a sprite at the old address cannot inherit world depth.
- [ ] **Check the near plane.** Move a triangle across the camera plane one vertex at
  a time. Record winding and clipping decisions. Any visible fragment must have finite
  coordinates and continuous UVs, including where an edge intersects the plane.
- [ ] **Separate camera jumps from vertex jitter.** Trace the view matrix while the
  player is stationary, walking, and returning from a minigame. Verify the first farm
  frame restores the projection center and camera transform together.
- [ ] **Check terrain against actors.** Walk a fixed path along a field edge and place
  an item on a tile. Compare ground, feet, and shadow positions while panning. Confirm
  the selected gameplay tile and collision results remain identical across both runs.
- [ ] **Resolve equal-depth cases.** Trace submission order while an actor passes
  behind a fence and under a foreground effect. Preserve the game's tie order for
  overlapping layers when evaluating a depth-based correction for opaque polygons.

### Texture sampling, rasterization, and display

Trace producers in [anim.c](src/main/graphics/anim.c), [texture_pack.c](src/main/graphics/texture_pack.c),
[clut.c](src/main/graphics/clut.c), and [screen_fade.c](src/main/graphics/screen_fade.c).
Use small synthetic pixel patterns alongside scene captures.

- [ ] **Quad diagonal and seams.** Put a checkerboard on an oblique quad and pan past
  it. Inspect the diagonal and its shared edge with the next tile for a discontinuity,
  a missing pixel, or a pixel blended twice. Compare opaque and translucent draws.
- [ ] **Texture page and palette addressing.** Exercise 4-bit, 8-bit, and direct-color
  draws with known texels at row and page edges. The final pixel must come from the
  expected texel and, for indexed data, the expected palette entry.
- [ ] **Sprite edges and texture windows.** Draw adjacent atlas cells with contrasting
  borders, including flipped sprites and a repeating window. Check the first and last
  columns for a missing texel, an extra column, or sampling from the neighboring cell.
- [ ] **Drawing area and offset.** Clip a primitive across each edge of the drawing
  area, then switch frame buffers. Check the last included pixel on each side and
  verify the offset is applied exactly once to both world and interface draws.
- [ ] **Dither coordinates.** Capture the same gradient at two window sizes and after
  a drawing-offset change. Compare at the logical render grid. Resizing the window
  must preserve the pattern attached to a given source pixel.
- [ ] **Color arithmetic.** Exercise black, nearly black, channel maxima, and a ramp
  across quantization boundaries. Compare raw texture and modulated draws to locate
  rounding, saturation, or a second color conversion before changing the shader.
- [ ] **Transparency and mask bits.** Use a known background and foreground for each
  PS1 semi-transparency equation. Check transparent texels, opaque black, and mask
  write/test separately. Preserve draw order through a weather effect and menu fade.
- [ ] **Palette and framebuffer hazards.** Change a palette, draw with it, copy a VRAM
  rectangle, then sample the result in the same frame. Compare first-use pixels,
  including after a menu closes. Keep the ordering of dependent copies and draws.
- [ ] **Presentation and pixel aspect.** Open a diary or dialogue at several window
  sizes. Check viewport centering, original 4:3 presentation, stable glyph baselines,
  and the absence of stale edge pixels after a resize.

### Frame pacing, input, and sound

Follow the [main loop](src/main/main.c), [pad polling](src/main/input/pad.c),
[SEQ player](native/seq_player.c), and [CD audio state machine](src/main/sound/cd_audio.c).
Reference the [SPU](https://psx-spx.consoledev.net/soundprocessingunitspu/) and
[CD controller](https://psx-spx.consoledev.net/cdromdrive/) for voice and stream behavior.

- [ ] **Simulation versus presentation.** Replay the same input on 60 Hz and a higher
  refresh display where available. Compare game ticks, walking distance, event step,
  and fade duration. Record duplicate or late presents separately from simulation drift.
- [ ] **Slow frames.** Introduce one bounded delay during a test transition. Trace the
  game clock, input edges, and audio tick through recovery. One press must trigger one
  action and queued events must retain their order.
- [ ] **Controller transitions.** Hold a button across a shop/menu transition, release
  it, then disconnect and reconnect at idle. Check pressed, held, and released states,
  menu repeat, and whether a stale input confirms a purchase or skips dialogue.
- [ ] **Original vibration requests.** Trace the known native rumble shortcut in
  pad handling on a compatible controller. Verify the requested motor envelope and
  stop behavior on scene exit or disconnect; ordinary input must work without rumble.
- [ ] **XA stream boundaries.** Switch from music to another XA stream and back while
  loading an area. Record file/channel selection, first audible sample, loop boundary,
  and stop completion. Listen for stale audio, gaps, or a click at the transition.
- [ ] **Music and effect ownership.** Trigger repeated tool sounds over sustained
  music, then pause and exit. Trace voice allocation, release, pan, and envelope.
  Compare an audio capture and ensure stopping the old scene releases its voices.
- [ ] **Audio clock drift.** Record game ticks, sequencer events, and delivered audio
  samples over a repeating passage. Compare at two host refresh rates and through
  a tempo change. Identify whether drift starts in scheduling, mixing, or presentation.

### Disc I/O, host memory, and parallel work

Start with [A_FILE reads](src/main/cd/afile.c), [frame storage](src/main/graphics/frame_buffers.c),
[overlay storage](src/main/overlay_regions.c), and [save writes](native/card_file.c).
Use repeated area transitions for the following measurements.

- [ ] **Cold and warm loads.** Time archive read, decode, texture upload, and the
  first visible frame separately. Compare the first farmhouse exit with a return visit.
  Optimize the stage that accounts for the measured delay.
- [ ] **Read completion and cancellation.** Trace an area read interrupted by a new
  scene request. Check the next read's offset, byte count, destination lifetime,
  and completion event before allowing a worker or cache to service the request.
- [ ] **GPU residency and invalidation.** Count texture and palette upload bytes on a
  stationary frame and after a palette edit. Reuse unchanged content and invalidate
  the affected region when the game writes it, including overlapping VRAM aliases.
- [ ] **GPU waits and readbacks.** Attribute each expensive wait to its consumer.
  For example, inspect the palette reads around `StoreImage` in `clut.c`. Compare
  wait duration and pixel output before and after reducing a redundant transfer.
- [ ] **Batch boundaries.** Count draw calls, texture switches, and flush reasons in
  the same scene. Group compatible submissions only where overlap, masking, and
  render-to-texture dependencies permit the original result.
- [ ] **CPU thread ownership.** If profiling identifies costly decode work, measure
  queue latency as well as decode time. Load another area before a job completes;
  the late result must never install data in the new scene or stall the audio callback.
- [ ] **RAM capacity and lifetime.** Record requested size, peak occupancy, and lifetime
  of primitive buffers, decoded assets, and overlay workspaces. Exercise the busiest
  recorded area under sanitizers and repeat entry/exit until warm memory stabilizes.
- [ ] **Pointer width and persistence.** Pair host allocation changes with a diary
  save, process restart, reload, and slot exchange. Verify the PS1-format payload size
  and restored fields. Exercise allocation failure with disposable saves.

### Which item to pick next

1. Start with the capture route and first rounding step. Record the earliest divergence
   and whether it also appears on PS1. A native-only rendering error takes priority.
2. Carry precision through one shared 3D producer and consumer. Once shared edges,
   packet reuse, and 2D isolation pass, use the same path for the other world geometry.
3. Use independent color, audio, or frame-pacing fixtures while geometry work proceeds.
   Pick host optimizations by measured frame cost, then rerun the same route and compare.

## 5. Close each fix

- [ ] Keep the smallest failing scene or synthetic fixture and record its before/after result.
  For precision corrections, document the intended visible difference from the PS1.
- [ ] Run the native Debug and Release suites and the affected gameplay route. Run the
  local PS1 matching check for changes covered by [AGENTS.md](AGENTS.md).
- [ ] Update the relevant row with evidence. Continue long-play coverage alongside development.
