# Platform status

As of 2026-10-26, native Windows replays complete the opening and validate free
player movement inside the house and in the farm after the Thomas/Zack dialogue.
Title/menu, farm, flashback, house and parts of the town render.
Full playthrough and broader gameplay remain unverified; see WINDOWS_PLAYTEST.md.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | All 113 original source units and native window build; tests passed (76/76 with -Game) |
| Windows x86-64 sanitizers | windows-asan-ubsan | All source units and native window build; Clang 20.1.8 ASan/UBSan tests passed (76/76 with pause and interpolation compiled); gameplay coverage remains limited |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
The owner now performs interactive gameplay testing. The Windows host stores
native card containers in Documents/BackToNature/saves and creates settings.json
only when missing (ADR-044). New isolated tests cover card persistence,
failed-write preservation, slot isolation, capacity, invalid containers, actual
Unicode filesystem paths and preserving existing settings. There are 66 base
tests plus one SDL filesystem test when the native window option is enabled.
Both release and sanitizer builds passed all 67 for the storage change. No gameplay
was launched for this storage change; in-game save/load remains owner-validated
work, not a proven result of the storage tests. Other platforms remain untested.
The subsequent Load-menu hang fix adds two original-source regressions, bringing
the base count to 68 and the native-window total to 69, all passing in release
and ASan/UBSan. Tests cover deterministic progress during asynchronous asset
waits, the combined save-image workspace, ordinary-card polling and background
callback types. The existing hung owner process was inspected with LLDB and
detached; no new gameplay or replay was run. Owner confirmation remains pending.
The owner subsequently confirmed the Load menu opens and reported an abort on
return to title. Title/save-load native state restoration is now implemented;
actual-object and integration units bring the count to 71 with the SDL test
(70 without it), all passing in release and ASan/UBSan. These are technical
checks, not an interactive title/load/return gameplay run. Remaining overlays
retain explicit unsupported-reentry failures.
On the subsequent pause branch, navigation, settings JSON and synthetic original
dialog-frame composition bring the count to 74 with the native pause build.
Release and ASan/UBSan pass those tests. An asset-only probe also decodes the
real English font (5645 nonzero pixels) and verifies startup VRAM is zeroed;
it runs no game loop or window. Physical ESC, audio pause/resume, monitor/mode
changes and visual presentation remain owner checks. Advanced renderer options
are explicitly pending; see ENHANCEMENTS.md and ADR-048.
VSync/FPS controls add a rational presentation-clock regression, bringing the
total to 75 in both Windows builds. It checks exact deadlines and unchanged
VBlank counts without starting gameplay. Native presentation caps at 30; the
original hardware tick stays 60 Hz. Higher caps duplicate images. Physical
VSync, achieved FPS and in-game time remain owner checks; see ADR-049.
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
The 12000-tick sanitizer idle test completed. Missing event-request pointer
returns explained the earlier release/sanitizer opening-script difference
(ADR-040); both now traverse the grandparent/flashback scenes and request XA.
Source-disc XA decoding/mixing has synthetic coverage (ADR-041). The 31000-tick
release and optimized sanitizer replays both complete the opening, respond to
right/down input and produce byte-identical final house BMP captures. This
does not establish original-hardware parity. A 39000-tick replay also completes
house exit, declines the tour, finishes Zack's dialogue and walks in the farm;
release/sanitizer final captures are identical (WINDOWS_PLAYTEST.md). Broader
gameplay still needs validation. Memory/scheduling regressions are
tracked through ADR-041. Unfinished SDK calls fail by
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

## Experimental visual interpolation validation

Date: 2026-10-26. Release and ASan/UBSan pass all 76 isolated tests with pause
and interpolation compiled. The native executable also builds with both compile
options OFF. No gameplay/window/replay was launched for this change. Tests
exercise private midpoint rasterization, current UI, history rejection and
byte-exact native VRAM/environment/packet preservation, with unchanged ticks.
Physical smoothness, scene coverage and achieved FPS remain owner checks.

Host: AMD Ryzen 5 4500, Windows x86-64, Clang 20.1.8 release (-O2, x86-64-v2).
Tool: hmbtn_interpolation_test --benchmark, C clock(), 10000 private replays.
The 320x240 synthetic fixture has only two small rectangles and a 1 MB private
VRAM copy. Measured 0.0212 ms/render in one warm run. This is a lower-bound
copy/replay cost, not representative of a game scene, full frame overhead or
an optimization comparison. The previous OFF path performs no private replay;
real GPU/CPU budget validation is still pending.
