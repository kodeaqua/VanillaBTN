# Enhance Scaling

Viewport-resolution geometry output, requested by the owner.
Build HMBTN_ENH_SCALING=ON (default OFF); Windows build.ps1 -Game compiles it.
Runtime enhance.scaling defaults false. Enable ESC > Settings > Graphics >
Enhance Scaling. The original textures stay original; this feature rerasterizes
triangle geometry at the active viewport size instead of enlarging its PS1
coverage. It does not create higher-resolution texture assets.

With Widescreen OFF, a 1920x1080 window has a centered 1440x1080 render viewport
and black side bars. Dimensions come from the actual window pixels, including
fullscreen/monitor changes. Current output backends support up to 4096 per
axis. Unsupported sizes or allocation failures disable scaling for the session
with a clear message; the stored preference is retained for the next launch.
Minimized, loading/direct, incomplete and unmatched scanouts fall back to native.

Read-only native frame observers capture two bounded command streams and their
pre-draw VRAM/environments. Without interpolation, replay the stream matching
the native displayed rectangle; cache repeated scanouts. With interpolation,
the existing private replay feeds the same generic command sink, using its
intermediate coordinates. Either enhancement can be disabled independently.
Loading/overlay changes invalidate history. No simulation, GTE, RNG, audio,
native VRAM or native GPU environment is advanced by extra presentations.

The sink implements flat/Gouraud/textured triangles and quads, rectangles,
sprites, native line-cell coverage, clipping, texture windows/CLUT sampling,
four semi-transparency modes, mask bits and inline image transfers/fills. It
uses the same native texture sampler and 15-bit color conventions. Texture
filtering remains nearest inside this initial rasterizer; the existing Bilinear
option filters final-frame presentation. Lines retain original logical-pixel
coverage. Framebuffer feedback textures still sample the private native VRAM;
they do not gain higher-resolution feedback. Optional per-texture bilinear is now shared with native output (ADR-055). FXAA/MSAA and expanded widescreen camera remain pending.

Software fallback executes the native rasterizer inside private replay to
preserve feedback/state semantics. GPU replay consumes output geometry directly. Output textures match image bounds, avoiding stale
border samples when bilinear presentation is used. Transfer buffers grow only
when required; textures change on output-size/path changes. No per-presentation
allocation occurs while the render size/path is stable. Pause UI retains its
original logical resolution and original font/dialog assets.

Tests compare supported 1x commands against the native rasterizer and verify
denser diagonal coverage at higher resolution, large-scale attribute safety,
unchanged native contexts/ticks, repeat scanouts, loading/overlay invalidation,
settings preservation and simultaneous interpolation. These are synthetic
technical tests; owner gameplay and display/device validation remain necessary.
Neither backend establishes actual high-FPS gameplay performance at 1080p or
above. See docs/port/PLATFORMS.md for measured synthetic costs.

GPU Auto now uses a shared Vulkan/D3D12 compute scene backend for both native and
enlarged output (ADR-054). Choose Renderer > Software for the CPU reference.
Native CPU VRAM/readback compatibility remains in normal game execution; GPU
output adds no game tick or readback stall. Unsupported scenes/operations fall
back to software. GPU fractional interpolation retains subpixel positions;
software scanout retains native coordinate quantization.
