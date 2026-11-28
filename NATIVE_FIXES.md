# Windows native correction checklist

The bring-up build is the starting point. These are investigation and implementation
tasks. Close each item with a repeatable comparison, a passing regression test,
or evidence that the game never exercises the behavior.
Keep 4:3 framing, original assets, gameplay ticks, and PS1-compatible diaries throughout.

## 1. Confirm what the game exercises

| Area | Observed in source | What to verify in the game |
|---|---|---|
| Projection | The model path runs GTE projection and writes packed XY into GPU packets. PsyZ's shader interpolates UVs with the same clip-space W for each vertex. [Model producer](src/main/geometry/model.c), [PsyZ vertex shader](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/shaders/psx.vert.glsl) | Record model and terrain jitter and texture drift. Follow the first rounding step in each producer. |
| Dithering and color | The game enables dithering in its draw environments. The SDL3 GPU path tags eligible primitives and applies a 4×4 matrix in the fragment shader. [Draw environments](src/main/main.c), [PsyZ fragment shader](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/shaders/psx.frag.glsl) | Verify the matrix phase, primitive eligibility, and quantization in the Windows build. Look for a difference before changing the existing dither path. |
| Mask bits | The draw decoder forwards GP0(E6h). The pinned hardware draw helper leaves nonzero mask write/test flags unimplemented. [GPU packet decoder](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/psyz/libgpu.c), [PsyZ draw helper](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_draw.h) | Trace actual packets and VRAM transfers. Establish whether a mask-sensitive draw occurs before scheduling a game fix. |
| Display modes | The game sets 320×240 and 640×480 display environments. The backend has a separate RGB24 path and ignores the interlace flag. [Display environments](src/main/graphics/screen_fade.c), [SDL3 GPU backend](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_gpu.c) | Record the mode bits the game actually requests. Schedule an interlace or RGB24 fix if a used mode produces a visible mismatch. |
| Primitive limits | VanillaBTN already tests the PS1 triangle size limits before handing a packet to PsyZ. A quad with one rejected half can be submitted via a temporary packet. [Packet filter](native/gpu_primitives.c), [Existing tests](native/test_gpu_primitives.c) | Verify both halves independently. Carry precise vertices through any packet copy or remap, including a partially rejected quad. |

## 2. Capture a baseline

- [ ] Record a slow camera pan in the farmyard, an oblique textured surface, an actor
  passing behind scenery, and a fade into a dialogue window. Record scene, camera,
  input sequence, frame number, viewport, and build revision. Keep captures local.
- [ ] Replay each sequence on the USA PS1 reference or DuckStation's software renderer
  at native resolution with enhancements disabled. Record the emulator settings.
  Mark each symptom as observed in both, native-only, or still unconfirmed.
- [ ] Add a menu/fade, a 640×480 display transition if used, and a minigame return.
  Record the draw/display environments and compare logical framebuffer pixels before
  window scaling. Capture both back buffers across each transition.
- [ ] In Release, capture CPU frame time, GPU frame time, present intervals, upload and
  readback bytes, and peak host memory on the same route. Record hardware and driver.
  Repeat after warm-up and compare median and 95th-percentile times.

## 3. Prioritize visible defects

| Priority and task | Where to investigate | Acceptance check |
|---|---|---|
| [ ] P1. Stabilize projected 3D vertices | PS1 projected screen coordinates are integers. Follow a vertex through the GTE calls in [model.c](src/main/geometry/model.c), packet construction, and PsyZ vertex submission. Preserve fractional screen coordinates and camera-space depth **before** packing XY for eligible 3D draws. | During the recorded pan, projected positions advance smoothly and shared edges stay joined. Check camera changes and minigame exit. HUD and sprites retain pixel alignment. |
| [ ] P1. Correct texture warping on 3D surfaces | The PS1 interpolates polygon UVs affinely. Once per-vertex depth survives projection, use perspective-correct interpolation for identified world polygons. Keep screen-space quads and sprite atlases on their existing path. | On the same sloping quad, a texture landmark stays attached during camera motion and across the triangle diagonal. Check UV boundaries, palette selection and flipped sprites. |
| [ ] P1. Diagnose popping and wrong occlusion | Trace near-camera geometry, winding, ordering-table insertion and [primitive clipping](native/gpu_primitives.c). Inspect opaque geometry separately from layered sprites, shadows, and semi-transparent draws whose result depends on submission order. | For each recorded disappearance or overlap, show which draw changed order or was culled. Verify the corrected frame and neighboring frames, including foreground scenery and effects. |
| [ ] P2. Correct dithering and color conversion | PS1 draw state can apply ordered dithering before RGB555 quantization. Compare the dither-enabled and disabled paths on a flat-to-shaded gradient. Check primitive eligibility, native pixel coordinates, and single application of quantization. | With dithering enabled, the 4×4 pattern has the reference phase and intensity. With it disabled, the same draw has no pattern. Check fades, unshaded sprites, text, and color band boundaries. |
| [ ] P2. Check blending, masks, and VRAM copies | Compare semi-transparent primitives over known background colors, palette uploads, and framebuffer copies. Follow PsyZ flushes when a draw samples a region written earlier in the frame. | Blend modes and mask-bit writes match reference pixel samples. Opening a menu or transitioning scenes shows the current texture and palette on the first visible frame. |

For the PGXP-style correction, carry projection data from the original geometry
producers into native draw submission. Verify subpixel XY, perspective UVs, and
visibility independently. Use a depth test only for draw groups that need it.

- [GTE projection and saturation](https://psx-spx.consoledev.net/geometrytransformationenginegte/)
- [GPU commands, color and blending](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/)
- [DuckStation's PGXP geometry, texture, culling and depth options](https://github.com/stenzek/duckstation/blob/master/src/duckstation-qt/graphicssettingswidget.cpp)

## 4. Use host GPU, CPU, and memory

The game and overlays already execute as x64 code. Debug and Release use PsyZ's SDL3 GPU
backend for rasterization and VRAM transfers. Native frame workspaces already use host RAM.
Use the hardware for work it can accelerate. Track frame deadlines, load latency,
and memory peaks as the success metrics.

| Resource | Task | Acceptance check |
|---|---|---|
| [ ] GPU and VRAM | Profile submissions, texture uploads, dirty-region synchronization, readbacks, and fence waits in the [pinned renderer](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_gpu.c). Keep reusable resources resident and batch compatible draws where the trace shows a cost. Preserve read-after-write dependencies and draw order. | The targeted wait or transfer decreases on the same route. Pixel probes, frame pacing, and GPU resource lifetime checks pass. |
| [ ] Host CPU | Profile game logic, packet preparation, asset decoding, and audio. If a measured decode or preparation task causes stalls, give a worker immutable input and publish its result at the existing game-thread boundary. | The stall decreases. Input, event order, game clock, and audio stay synchronized at different monitor refresh rates and during area changes. |
| [ ] Host RAM | Measure peak use and buffer occupancy in [frame buffers](src/main/graphics/frame_buffers.c), [overlay regions](src/main/overlay_regions.c), and [archive reads](src/main/cd/afile.c). Replace a limiting workspace or cache only after identifying its lifetime, owner, and size bound. Keep disc offsets, VRAM addresses, and serialized save fields in their original formats. | Repeated farm, menu, and minigame transitions reach a stable memory plateau. Allocation failure leaves saves intact. Sanitizers and diary round-trips pass. |

- [ ] **Verify the GPU path.** Confirm the SDL GPU driver and actual adapter in a
  Windows capture. Identify time spent in rasterization, copies, and CPU waits.
  Check that ordinary rendering and any color fixes execute on the GPU.
  [SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU) ·
  [PIX GPU captures](https://devblogs.microsoft.com/pix/gpu-captures/)
- [ ] **Find the limiting CPU thread.** Attribute running time and blocked time to
  gameplay, packet preparation, CD/archive reads, or audio. Use the trace to choose
  between removing a wait, reducing CPU work, and scheduling independent work.
  [PIX timing captures](https://devblogs.microsoft.com/pix/timing-captures-new/)
- [ ] **Account for memory by owner.** Record process committed bytes and GPU resource
  bytes alongside frame, asset, and audio allocations. Size native storage from peak
  live demand. Set a bound for any cache and verify eviction and allocation failure.
  [Windows Performance Recorder resource profiles](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/recording-for-resource-based-analysis)

## 5. Work queue by subsystem

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
- [ ] **Check GTE saturation and lighting.** Compare screen coordinates, depth, flags,
  and depth-cue color for near-plane, offscreen, and high-intensity inputs used by the
  game. Keep the arithmetic that controls gameplay in parity when increasing draw precision.
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
- [ ] **Dither eligibility and Windows shaders.** Toggle the draw-environment bit for
  flat, Gouraud, raw-textured, and modulated polygons, lines, and sprites. Compare
  all 16 positions of the pattern and the quantized levels in the D3D12 output.
  If shaders change, verify the embedded DXIL was regenerated and used by that build.
  [PsyZ shader build script](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/shaders/build_shaders.sh)
- [ ] **Color arithmetic.** Exercise black, nearly black, channel maxima, and a ramp
  across quantization boundaries. Compare raw texture and modulated draws to locate
  rounding, saturation, or a second color conversion before changing the shader.
- [ ] **Transparency and mask bits.** Use a known background and foreground for each
  PS1 semi-transparency equation, including odd RGB555 values and repeated overlaps
  that reveal rounding drift. Check transparent texels, opaque black, and mask
  write/test separately. Preserve draw order through a weather effect and menu fade.
- [ ] **Palette and framebuffer hazards.** Change a palette, draw with it, copy a VRAM
  rectangle, then sample the result in the same frame. Compare first-use pixels,
  including after a menu closes. Keep the ordering of dependent copies and draws.
- [ ] **Presentation and pixel aspect.** Open a diary or dialogue at several window
  sizes. Check viewport centering, original 4:3 presentation, stable glyph baselines,
  and the absence of stale edge pixels after a resize.
- [ ] **Display and draw buffer handoff.** Trace display origin, draw offset, clears,
  and the first shown frame when changing display environments. Check for stale VRAM,
  a one-frame flash, or a wrong field/stride where the game requests such a mode.
- [ ] **Line and transfer edges.** Compare one-pixel line endpoints and joins, odd-width
  uploads, and overlapping copies at the VRAM edges used by the game. Compare destination
  words as well as the rendered frame, including after the next draw samples the copy.

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
- [ ] **SPU mix fidelity.** Compare a captured note and effect at several pitches and
  volumes. Isolate ADPCM loop points, envelope and release, interpolation, reverb sends,
  and clipping when multiple voices overlap. Fix the first stage with a reproducible mismatch.

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
  recorded area under sanitizers and repeat entry/exit until warm memory stabilizes. If
  a PS1-era workspace cap causes a drop or stall, grow the native allocation before
  issuing references into it and check their lifetime through frame completion.
- [ ] **Pointer width and persistence.** Pair host allocation changes with a diary
  save, process restart, reload, and slot exchange. Verify the PS1-format payload size
  and restored fields. Exercise allocation failure with disposable saves.

### Implementation order

1. Capture graphics and host timing on the same route. Triage reproduced port defects
   first. Start renderer pixel checks with a 4×4 dither fixture and a blend/VRAM probe.
2. Trace the first rounding step and carry precision through one shared 3D path.
   Once shared edges, packet reuse, and 2D isolation pass, use it for the other world geometry.
3. Fix confirmed timing, audio, and rendering issues with their own fixtures. Address
   bottlenecks that miss frame deadlines or delay transitions, then compare the same route.

## 6. Close each fix

- [ ] Keep the smallest failing scene or synthetic fixture and record its before/after result.
  For precision corrections, document the intended visible difference from the PS1.
- [ ] Run the native Debug and Release suites and the affected gameplay route. Run the
  local PS1 matching check for changes covered by [AGENTS.md](AGENTS.md).
- [ ] Recheck pixel values and gameplay ticks after GPU, CPU, or RAM changes. Report
  before/after frame times, loads, or peak memory for the metric the change targets.
- [ ] Update the relevant row with evidence. Continue long-play coverage alongside development.
