# Shared output capture

Native scene output and enhancements consume the same read-only frame replay.
frame_replay owns two bounded native command streams, pre-draw VRAM and GPU
environments. Clients acquire/release a shared capture independently; enabling
scaling is not a prerequisite for a native output backend. The selected stream
matches native front scanout. Direct/loading, expired or missing captures fail
closed. Private execution restores the native contexts before returning.

replay_sink supports ordinary reference execution, backend-owned geometry
consumption and optional fractional motion metadata. GPU packet construction
consumes private geometry without rasterizing it a second time on the CPU;
embedded polygon tpages still update the private environment. The game's normal
low-resolution CPU VRAM remains authoritative for reads, transfers and logical
compatibility. This change does not remove that compatibility work.

gpu_scene emits an output-independent command representation with a 16-word
header, packed 16-bit VRAM snapshot, 40-word primitive records and 16x16 logical
tile lists. Each tile has at most 256 ordered references; a scene has at most
8192 primitives and 2048 tiles. All records use explicit uint32_t fields and
byte offsets are bounded by HM_SCENE_MAX_WORDS. No disc data is interpreted as
one of these records. Material records include CLUT/tpage, texture window,
clipping, display-write controls, dither and mask flags. Lines become logical
cells (flat axis-aligned lines are compact rectangles). Quad triangle order
and shared-edge ownership are retained.

Output backend execution is separate from this portable capture/packet layer.
Snapshots are immutable input; only backend-owned output is written. Source
texture/CLUT overlap with output, offscreen draw dependencies, dependent VRAM
transfers and capacity overflow reject GPU packet output so the caller can use
the software reference. Fully offscreen untextured draws are skipped and their
conservative 16x16 VRAM tile footprints are tracked; later textured draws trigger
software fallback only when sampled textures or CLUTs overlap those written tiles. Leading fills/uploads and transfers outside both the
output and previously sampled texture/CLUT tiles update the private initial
snapshot. Tile-level overlap is conservative and retains fallback when a
dependency cannot be excluded.

packet_identity records construction spans without changing the packets.
Native-only adapters identify field/greenhouse sprites by the projected world
cell, rather than a work-buffer offset which changes when other sprites are
allocated. The registry resets with the main OT and is bounded; ambiguous
spans are rejected. Interpolation additionally groups polygons by shared
normalized screen vertices. Missing or conflicting history makes the entire
connected component current, avoiding cracks between mixed frame positions.
Unconnected sprites retain independent interpolation. Fractional metadata can
be consumed by an output backend while CPU reference scanout keeps its original
pixel quantization. See ADR-053.

Shared SDL_GPU execution lives in platform/gpu_renderer.c (ADR-054). GPU Auto
attempts the shared scene renderer on every desktop frame, at native and
viewport resolutions independently of scaling (ADR-138). Native pixel math is
integer; optional fractional motion/enlarged coverage use fractional fields.
Android retains its selective compute policy because of the mobile TBDR
bottleneck recorded in ADR-132. SPIR-V/DXIL come from our HLSL under shaders/.
Default builds use checked-in arrays; build-shaders.ps1 regenerates with pinned
DXC. CMake rejects stale normalized source hashes. Unsupported cases retain
software output.

Optional widescreen (ADR-056) expands the private logical render area, not native
VRAM or GTE state. Shared replay now preserves world/UI group boundaries;
interpolation forwards each command's group. World clips expand while ordinary
UI stays on its original canvas. Full-screen flat F4 fades/fills cover the wider
output. GPU initial pixels outside native framebuffer bounds are black; feedback
checks retain native bounds. Near-boundary enlarged triangle coverage uses exact
Q8 edge signs via 32-bit word arithmetic. See enhance/widescreen/README.md.

ADR-057 keeps all-screen widescreen presentation independent of replay success.
Generic background spans identify original title tiles without marking logos or
text as background. Sampled UV footprints and actual clipped writes now decide
feedback/offscreen rejection. platform/gpu_present.c is the shared compositor
for direct/native and expanded output; offscreen probes compare both paths.
Auto fallback uses native internal size; explicit Software keeps the enlarged
reference raster. World projection and game clocks remain unchanged.
