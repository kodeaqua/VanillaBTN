# Platform status

As of 2026-10-26, the native Windows title/menu, farm and opening grandparent
dialogue render. The opening sequence and free player control are still under
validation; this is not yet a fully playable port.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | All 113 original source units compile; local tests passed (63/63) |
| Windows x86-64 sanitizers | windows-asan-ubsan | All source units compile; Clang 20.1.8 ASan/UBSan tests passed (63/63); execution coverage remains limited |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
Windows validation compiles all original translation units and links the native
boot diagnostic. The full-tree syntax inventory passes 113/113; warnings remain
visible and do not establish runtime safety. Strong game definitions have no
collisions, excluding compiler COFF reference entries. The console probe ran the
original C entry point with the verified USA disc: callbacks, virtual VSync,
silent audio setup, A_FILE initialization, draw/display setup and GTE/GS setup
execute, followed by no-card initialization, title init and streamed VAB loading.
Native sequencing/voices/reverb produce PCM, and an actual SDL audio callback
consumes output. The Vulkan/ASan window test completed 1200 VBlanks and captures
show the title/menu/dialogue. A later Vulkan/ASan window replay completed 6000
VBlanks and its capture shows the farm terrain, player and HUD. The release
probe reaches the opening grandparent dialogue after naming/birthday setup.
The 12000-tick sanitizer idle test completed, but release/sanitizer opening
script behavior differs and remains under investigation. Movement and later
scene changes still need validation. Memory/scheduling regressions are tracked
through ADR-038. Unfinished SDK calls fail by
name, not fake
success paths. Native overlay reentry also aborts until state reset is supported.
The original
frame/farm setup and region helpers are now tested using native data storage.
Normal and larger-work-area layouts, pointer bounds, input-demo preservation,
GS work allocation boundaries and signed free-space budgets pass under ASan/UBSan.
The workspace test observes GPU/GS setup calls. A separate gpu.links test now
uses actual reverse OT initialization and the guarded original insertion/builders
to validate pointer handles, packet order/lengths, cursor movement and re-clearing.
Malformed handles and registry exhaustion fail explicitly. GPU tests exercise
texture/CLUTs, affine quad endpoints, Gouraud winding, dithering, line endpoints,
polyline termination and shared-edge blending through real DrawOTag. Exact
hardware raster rules and golden-frame parity still need validation.
CD position helpers are tested over all 450000 valid two-digit-minute BCD
positions. Disc/parser and SDK-facing reads now pass synthetic tests, including
deterministic tick progression and IO failures. No disc is needed by CTest.
The local disc probe passed with the owner's SHA-1-verified USA raw track in
release and ASan/UBSan: four startup files read, 32 archive ranges checked.
GPU upload/readback/fill and the original raw/row-RLE/column-RLE texture decoder
are tested against native 1024x512 VRAM. There is no displayed frame yet.
Eight lexical-audit/helper regression tests also passed using a local Python 3.11
interpreter. Clang 19 ASan failed during startup; the pinned Clang 20 upgrade
resolved that startup failure (ADR-005).
The optional SDL3 3.2.28 GPU spike passed hardware upload, nearest blit, byte-exact
readback and eight-frame presentation via Vulkan on Radeon RX Vega, driver
31.0.21925.1001. It limits frames in flight to two. D3D12/Metal and other devices
remain untested. This synthetic spike is separate from game rendering and does
not establish frame-time budgets. See GPU_TESTING.md and ADR-018.
The initial budgets remain targets: 60 Hz game ticks, port overhead below 2 ms
desktop / 4 ms mid-range Android, and desktop cold start below 3 seconds after
initial cache creation. Full-game measurements do not exist yet. LTO, CPU baselines, stripping,
static Windows CRT and packaging must be validated when a game target is added.

## Synthetic OT construction measurement

Date: 2026-10-25. Host: AMD Ryzen 5 4500 6-Core Processor, Windows x86-64.
Tool: `hmbtn_ot_bench` using C clock(), Clang 20.1.8 windows-release (-O2,
x86-64-v2). No sanitizer, LTO or renderer is involved. Addresses are registered
once before timing; each of 100000 repetitions clears a 2048-word OT and inserts
1000 persistent three-word packets. The code performs no per-iteration allocation.

Three runs: 799, 799, 798 ms total, or 0.007990, 0.007990, 0.007980 ms per
construction (approximately 8 microseconds). This is a warm, repeated CPU tag
workload measured with a coarse clock on a non-isolated host, not game frame
time. Payload generation, chain submission, rendering, GTE, audio and platform
work are outside the measurement. There is no valid previous native rendering
baseline for a before/after comparison; no optimization or frame-budget claim
is made. Full playthrough/device profiling is still needed to validate the
65536-live-tag registry capacity and its actual game cost.
