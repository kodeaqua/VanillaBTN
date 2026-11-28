# Windows native correction checklist

Keep 4:3 framing, original assets, gameplay ticks, and PS1-compatible diaries throughout.
Use the matching USA PS1 build as the behavior reference and the current Windows
build as the performance baseline.

## Delivery order and acceptance

| Stage | Work | Evidence to advance |
|---|---|---|
| 0. Make comparisons repeatable | Capture the title demo on both targets. Run the first RGB555 and VRAM probes through the Windows GPU backend. Measure a Release route. | A second run reaches the same checkpoints. Record expected and observed results for both pixel probes, plus the range of baseline frame and load times. |
| 1. Restore PS1 behavior | Fix confirmed native differences in rendering, timing, input, and audio. Prioritize crashes, state loss, and broken transitions ahead of small pixel differences. | Each fix reproduces before the change and passes afterward. Check exact VRAM words for pixel fixtures and exact tick/event order for gameplay probes. |
| 2. Correct PS1 geometry artifacts | Preserve subpixel XY for one 3D producer, carry depth for perspective UVs, and evaluate clipping and draw order separately. Extend to other producers after the first path passes. | The 3D fixtures pass, HUD and sprite probes match the native baseline, and gameplay checkpoints match. Each deliberate difference from the PS1 reference has a recorded expected result. |
| 3. Remove measured host bottlenecks | Pick the readback, upload, CPU work, or native workspace that accounts for a missed deadline or slow load. Use the same route for the comparison. | The target improves beyond baseline variation, with the same sampled pixels, gameplay checkpoints, and save round-trip. Memory settles across repeated transitions. |
| Phase complete | Run the route matrix in Windows Debug and Release. Track remaining items against the game commands or scenes they affect. | All reproduced in-scope defects have a passing check. Debug routes report zero sanitizer errors. Record the reason and trigger for revisiting any unused or out-of-scope path. |

A host improvement may land as soon as the affected path has a repeatable baseline
and passes its correctness checks. Investigate features such as mask writes or
interlace where a game route or reachable command stream shows a dependency.

## 1. Confirm what the game exercises

| Area | Observed in source | What to verify in the game |
|---|---|---|
| Projection | The model path runs GTE projection and writes packed XY into GPU packets. PsyZ's shader interpolates UVs with the same clip-space W for each vertex. [Model producer](src/main/geometry/model.c), [PsyZ vertex shader](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/shaders/psx.vert.glsl) | Record model and terrain jitter and texture drift. Follow the first rounding step in each producer. |
| Dithering and color | The game enables dithering in its draw environments. The SDL3 GPU path tags eligible primitives and applies a 4×4 matrix in the fragment shader. [Draw environments](src/main/main.c), [PsyZ fragment shader](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/shaders/psx.frag.glsl) | Verify the matrix phase, primitive eligibility, and quantization in the Windows build. Look for a difference before changing the existing dither path. |
| Mask bits | The draw decoder forwards GP0(E6h). The pinned hardware draw helper leaves nonzero mask write/test flags unimplemented. [GPU packet decoder](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/psyz/libgpu.c), [PsyZ draw helper](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_draw.h) | Trace actual packets and VRAM transfers. Establish whether a mask-sensitive draw occurs before scheduling a game fix. |
| Display modes | The game sets 320×240 and 640×480 display environments. The backend has a separate RGB24 path and ignores the interlace flag. [Display environments](src/main/graphics/screen_fade.c), [SDL3 GPU backend](https://github.com/Xeeynamo/psyz/blob/00fb42e6f49c146ad760a1f605b15d2a4ec598f0/psyz/src/platform/sdl3_gpu.c) | Record the mode bits the game actually requests. Schedule an interlace or RGB24 fix if a used mode produces a visible mismatch. |
| Primitive limits | VanillaBTN already tests the PS1 triangle size limits before handing a packet to PsyZ. A quad with one rejected half can be submitted via a temporary packet. [Packet filter](native/gpu_primitives.c), [Existing tests](native/test_gpu_primitives.c) | Verify both halves independently. Carry precise vertices through any packet copy or remap, including a partially rejected quad. |

## 2. Capture a baseline

| Route | Setup | Compare |
|---|---|---|
| Title demo | Fresh boot, leave the title idle with no controller input. Match the opening scene and input sequence to identify the same recording on both targets. | From first replay tick through return to title, align captures by demo tick. Compare the camera, moving edges, texture landmarks, fade duration, and scene order. [Demo update](src/overlay/11_GF_FARM/demo.c), [Replay seed](src/overlay/11_GF_FARM/farm_frame.c) |
| Farm and dialogue | Start from an equivalent diary copy. Walk the same path past a fence with the player behind it, then trigger the same dialogue or menu and close it. | Compare the first visible frame, shared edges, sprite alignment, depth order, text, palette, and fade. Record scene, area, camera and update tick. |
| Minigame and return | Enter a minigame through normal gameplay so the route works on both targets and in Release. Repeat entry and exit. Cover all five by the phase exit. | Compare projection, first frame after a transition, scene/event order, and restored farm camera. Record display mode, origin, draw offset, and both back buffers if the mode changes. |
| Load and persist | Leave the farmhouse, re-enter and repeat, then save a disposable test diary, restart and reload it. | Time archive read, decode, upload, and first visible frame. Check restored fields, slot selection, and PS1 diary exchange when the change touches storage. |

- [ ] Record the VanillaBTN, PsyZ and SDL revisions, build type, GPU adapter and driver,
  display refresh rate, starting state, and input/tick range. For DuckStation, record
  the version and use software rendering at native resolution with enhancements disabled.
- [ ] Compare logical pixels before window scaling. Keep local captures and traces
  with the reproducer. Record expected and observed values, reference type, and whether
  the difference exists only in native or also on PS1.
- [ ] Run the initial checks in the table below. Use synthetic commands and sample the
  logical target with `StoreImage`. Wait for `DrawSync(0)` before reading the result.
  Keep generated test data independent of disc assets.
  [Packet test](native/test_gpu_primitives.c)

### First regression probes

| Probe | Input and observation | Pass condition |
|---|---|---|
| Dither phase and quantization | Draw a constant-color Gouraud polygon at a safe interior VRAM location, using channels just on and around a multiple of eight. Read the 16 positions of one 4×4 tile with dithering on and off. Repeat at an offset in each axis. | Add the documented per-pixel offset to each 8-bit input, clamp to 0–255, and truncate to 5 bits. Use a zero offset with dither off. Verify eligibility separately for raw/modulated textures, lines and rectangles. [PS1 dither rules](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#24bit-rgb-to-15bit-rgb-dithering-enabled-in-texture-page-attribute) |
| Blend and VRAM visibility | Draw all four blend modes over known RGB555 backgrounds with odd channel values. Upload a new CLUT entry and copy the destination to another VRAM region, then sample both updated regions in the same frame. | Readback matches the reference words at the affected coordinates. Both dependent draws use the updated data, including on the first frame. [PS1 blend and transfer rules](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/) |
| Projection and UVs | Move two adjacent world triangles by fractions of a logical pixel and use an oblique checkerboard with different vertex depths. Submit a nearby HUD sprite and reuse packet storage on the next frame. | Projected XY follows the calculated fractional positions, the shared edge stays joined, and the texture landmarks follow their expected perspective positions. HUD pixels and the next frame's unrelated draw match their own inputs. |

On Windows D3D12, `vanillabtn_gpu_pixels` checks the Gouraud dither phase, one
RGB555 vector across all four blend modes, and VRAM copies and CLUT updates
sampled in the same frame. [GPU pixel test](native/test_gpu_pixels.c)

- The pinned shader produced `0x7ac7` instead of `0x7aa6` for the mode 3
  vector. Truncating the quarter foreground in RGB555 fixes that vector.
  [Shader patch and regeneration](native/shaders/README.md)
- D3D12 readback verifies sprite U/V wrap at 255 and right-to-left VRAM wrap
  for 16-bit textures and 8-bit textures/CLUTs. Same-frame cases also verify
  that a draw on the left edge is visible to the wrapped texture read.
- The D3D12 probe checks that raw Gouraud-textured polygons preserve RGB555
  texels and that modulated Gouraud-textured polygons apply the dither matrix.
- Keep flat polygons, lines, rectangles, and repeated blending open until
  their probes run.

### First title-demo baseline

- Beetle PSX `0.9.44.1 b76bfc5` produced identical software-rendered captures
  across two runs at 10 demo ticks. A native diagnostic confirmed the same
  recorded inputs and sampled its logical framebuffer at those 10 ticks.
- From demo tick 120 through 1680, native advanced 1,560 game updates and the
  reference advanced 3,120 VBlanks. The full demo reached tick 1848 on both.
  Match the logical crop and presentation phase before using image diffs as a gate.
- Fresh Windows game Debug and Release builds passed all seven CTest tests
  after the blend fix. Both booted the verified USA image on D3D12. The
  changed shader has a separate readback test for its exact RGB555 result.

Three Release runs of `b92fb8b` used the Radeon RX Vega, D3D12, and a 75 Hz
display. PresentMon 2.6.0 sampled the common 15–60 second title window.
Times below are ranges of the individual run statistics.

| Metric | Median range | 95th percentile range |
|---|---:|---:|
| Frame time | 26.90–33.25 ms | 47.51–52.03 ms |
| PresentMon CPU busy | 22.39–27.91 ms | 33.04–33.26 ms |
| GPU busy | 0.147–0.160 ms | 0.260–0.303 ms |
| Displayed time | 26.70–26.73 ms | 53.35–53.39 ms |

Process private bytes peaked at 814.1–814.2 MiB in the once-per-second
samples over the full runs. Use this title interval for regression comparisons
on this host and collect separate samples for farm and minigame workloads.

- A separate instrumented Release run reached the first replay tick 2.018 s
  after the title requested the farm. The farm overlay A_FILE read took 53.6 ms.
  Profile section reads, decoding, and upload separately to attribute the rest.
- At 70.23 s after the first present, that run had submitted 1,978,108 texture
  upload bytes and 12,853,272 GPU buffer upload bytes. The texture readback and
  fence-wait counters were zero on this route through that checkpoint.

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

### Benchmark protocol

- Use Release with the same adapter, driver, tracing, window settings, and refresh
  rate. Report first-load results separately. Warm the route once, then take at
  least three paired baseline/candidate measurements over the same tick range.
- Report median and 95th-percentile frame times by CPU work, GPU work, and present
  interval. Report transfer bytes, slow load intervals, and committed memory peaks.
  Record simulation ticks, vertical blanks, and presents as separate counters.
- Choose the target metric from the baseline. The difference between baseline and
  candidate medians of that per-run metric should exceed the baseline's max–min
  spread. Check pixels and tick counts and report other measurable regressions.

### Resource choices

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

## 6. Close each fix

- [ ] Keep the smallest scene or synthetic fixture that fails before the fix and passes
  after it. For an intentional correction, record the expected 3D output and the
  unchanged gameplay checkpoints. For parity defects, use the PS1 reference output.
- [ ] Keep GPU state and shader fixes in the renderer, geometry precision in the
  producer/submission path, and scheduling fixes in the host layer. Record any PsyZ
  revision change and regenerate and test the Windows DXIL for shader changes.
- [ ] Run the native Debug and Release suites and the affected gameplay route. Run the
  local PS1 matching check for changes covered by the project rules. Require all 17
  artifacts with a disc or all 14 code binaries without one. [AGENTS.md](AGENTS.md)
- [ ] Recheck pixel values and gameplay ticks after GPU, CPU, or RAM changes. Report
  before/after frame times, loads, or peak memory for the metric the change targets.
- [ ] Update the relevant row with evidence. Leave unverified items open and record how
  to reach them. For an unused path, record the command/scene coverage that supports
  that decision. Continue long-play coverage alongside development.
