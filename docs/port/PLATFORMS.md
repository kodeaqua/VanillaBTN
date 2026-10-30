# Platform status

As of 2026-10-26, native Windows replays complete the opening and validate free
player movement inside the house and in the farm after the Thomas/Zack dialogue.
Title/menu, farm, flashback, house and parts of the town render.
Full playthrough and broader gameplay remain unverified; see WINDOWS_PLAYTEST.md.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | All 113 original source units and native window build; tests passed (79/79 with -Game) |
| Windows x86-64 sanitizers | windows-asan-ubsan | All source units and native window build; Clang 20.1.8 ASan/UBSan tests passed (79/79 with shared GPU output, pause, interpolation and scaling compiled); gameplay coverage remains limited |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Gradle / NDK r27c, minSdk 26 | Previous build succeeded; current device behavior remains unverified |

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

The subsequent FPS Limit Off addition extends the existing clock regression:
600000 uncapped visual attempts still give 600 ticks for 300 VSync(2) calls.
Pause composition also verifies dimmed background RGB with full-bright dialog
and glyphs. These changes keep the total at 76 tests; physical FPS and actual
pause appearance remain owner checks. The active owner release process was
left running, and the final release link was written separately to
build/port/windows-pause-preview/hmbtn_native.exe because Windows locks its
running executable. Rebuild after closing the game to update windows-release.

## Viewport-resolution scaling validation

Date: 2026-10-26. Release and ASan/UBSan pass 77/77 isolated tests with scaling,
interpolation and pause compiled. The release executable was rebuilt after the
owner closed the previous process; the earlier locked-executable note is
historical. Feature-OFF and scaling-ON/interpolation-OFF native targets also
link. No game/window/replay was launched. Other platforms and actual display
resource replacement remain untested. The software target is capped at 4096
per axis; allocation failure visibly falls back to native rendering.

Host: AMD Ryzen 5 4500, Windows x86-64, Clang 20.1.8 release (-O2, x86-64-v2).
Tool: hmbtn_scaling_test --benchmark, C clock(), 50 warm passes per size.
One flat triangle spans much of a synthetic 16x16 logical area transformed to
the output dimensions. Private native command execution is included, but this
is not a game scene, textured workload, GPU frame or full port overhead.

| Output | Initial implementation | After mapping/flat-color changes, three runs |
| --- | --- | --- |
| 320x240 | 1.080 ms | 0.280 / 0.300 / 0.300 ms |
| 640x480 | 4.340 ms | 1.140 / 1.140 / 1.160 ms |
| 1440x1080 | 22.000 ms | 5.800 / 5.860 / 5.780 ms |

The initial measurements exposed repeated per-pixel coordinate division and
constant-color interpolation. Mapping tables and a constant-color branch avoid
that work; supported 1x raster comparisons remain exact. These coarse warm
measurements show only this synthetic cost reduction, not achieved gameplay FPS
or representative headroom. Texturing, overdraw and interpolation can cost
substantially more. Repeated non-interpolated scanouts are cached. A GPU scene
raster path remains the next performance step before claiming high-resolution
high-FPS support.

## Shared replay and ground-seam regression

Date: 2026-10-26. Integration builds pass 79/79 release and ASan/UBSan tests.
Portable packet tests verify reference-counted replay capture, native-context
preservation and bounded output rejection. A connected-mesh regression against
the f93bb01 interpolation object fails at a visible black gap; the corrected
implementation passes. Stable field-cell identities survive inserted work
packets. The owner isolated actual farm cuts to Interpolation ON, but has not
yet confirmed this correction in gameplay. No game session/replay was launched.

## Shared GPU output, offscreen validation

Date: 2026-10-26. Host: Ryzen 5 4500 / Radeon RX Vega, driver 31.0.21925.1001.
Clang 20.1.8 release (-O2, x86-64-v2); SDL 3.2.28. Offscreen compute probes pass
on Vulkan and D3D12. No window, disc, game loop or input replay is used. Native
RGB is byte-exact against the software reference for fixed fixtures and 2048
deterministic random primitives. Tests cover masks, blending, texture depths/
windows, clipping, lines, display-write control and both framebuffer origins.
High-resolution comparisons allow up to 9/255 per color channel (one 5-bit step);
fractional sprite motion across a tile boundary has a separate exact check.
Game scene coverage and full playthrough remain owner validation.

Timing tool: hmbtn_gpu_renderer_probe <driver> --benchmark, SDL_GetTicksNS,
50 passes per size, one large flat triangle over a synthetic 32x32 logical area.
CPU includes private VRAM restore, output seeding and reference raster. GPU
includes restore, packet creation, upload, compute and a fence wait. No output
readback, presentation, game logic, audio or full-scene overdraw is timed. Normal
runtime does not perform this benchmark fence wait.

| Output | CPU / Vulkan-run | GPU / Vulkan | CPU / D3D12-run | GPU / D3D12 |
| --- | --- | --- | --- | --- |
| 320x240 | 0.344 ms | 0.484 ms | 0.382 ms | 0.566 ms |
| 640x480 | 1.246 ms | 0.569 ms | 1.324 ms | 0.616 ms |
| 1440x1080 | 6.198 ms | 0.644 ms | 6.360 ms | 0.882 ms |

These warm single-run synthetic numbers show a high-resolution benefit and
small native-resolution overhead. They are not achieved gameplay FPS or a port
overhead guarantee. Native CPU VRAM compatibility is still maintained. GPU Auto
conservatively falls back on unsupported dependencies/transfers or capacity.
The owner's active windows-release executable remains untouched; a separately
linked windows-gpu-preview executable contains the latest changes while that
process is open. Owner gameplay confirmation of the field-seam fix is pending.

### Optional texture filtering (2026-10-26)

ADR-055 adds palette-aware bilinear in GPU and private software output, independent
of scaling. Native VRAM and game clocks remain authoritative and unchanged.
Release/ASan/UBSan: 80/80 tests. Offscreen Vulkan and D3D12 pass native exact
and enlarged tolerance comparisons with Nearest/Bilinear. Filtering-ON with
scaling-OFF also compiles. No game was launched, no gameplay FPS measurement
was made, and atlas behavior in actual gameplay remains owner-tested.
No decomp-side source or matching guard changed.

### Widescreen technical checks (2026-10-26)

Expanded world/UI and full-screen fade synthetic readback pass on Vulkan and
D3D12, including Nearest/Bilinear, native-height/enlarged output and y=0/240
framebuffer origins. No window or gameplay was launched. OFF source guards
reconstruct the original files exactly; matching binaries remain unverified
because the Linux matching toolchain is unavailable and WSL is prohibited.

Local Radeon RX Vega, Release, SDL_GetTicksNS, 25 serialized renders of a tiny
synthetic wide scene (two textured sprites, one large flat triangle, UI and full
screen fade), 1920x1080:

| Driver | Filtering | CPU ms/render | GPU capture/upload/compute/fence ms/render |
|---|---|---:|---:|
| Vulkan | Nearest | 30.064 | 1.310 |
| Vulkan | Bilinear | 31.202 | 1.322 |
| D3D12 | Nearest | 30.126 | 1.397 |
| D3D12 | Bilinear | 31.080 | 1.338 |

These are synthetic output costs, not gameplay FPS or complete frame costs.
Probe fences serialize GPU work; normal presentation adds no fence/readback.
Different rows include timing noise; no filtering speedup is inferred. Owner
playability, all-map coverage and weather/transition behavior remain unverified.

Widescreen validation also passes 82/82 Release and 82/82 ASan/UBSan unit tests.
A native CPU-only build with Widescreen ON, GPU/Scaling/Filtering OFF succeeds;
a native build with Widescreen OFF also succeeds. The release game process was
left running; a separate windows-wide-preview executable was linked for owner
manual testing instead of replacing the locked release executable.

### All-screen widescreen and dialogue fallback (2026-10-26)

ADR-057 removes the world-only viewport decision and preserves full-window
presentation across 2D/loading/pause/native fallback. Title background tile
spans expose existing assets; logos and text keep their native proportional
canvas. Offscreen Vulkan/D3D12 compare presentation alternating expanded GPU,
native fallback and Widescreen OFF. No gameplay was launched for these checks.

Before the fix, a synthetic oversized text clip fails GPU scene construction
at gpu_scene.c's new regression assertion. The same clip with visible glyphs
now succeeds; genuinely offscreen writes/texture feedback still reject. Sampled
UV rows outside a framebuffer now succeed even when their enclosing page
intersects it. This proves the technical triggers, not the owner's exact trace.

Release, Radeon RX Vega, SDL_GetTicksNS, 25 serialized output renders. Workload:
two textured sprites, a flat world triangle, UI/fade, 40 synthetic 8x12 glyphs,
an oversized dialogue clip and restored native clipping. Native retry is the
new Auto fallback policy; viewport-sized CPU retry is the previous policy:

| Driver | Filter | Previous CPU retry 1920x1080 ms | Native CPU retry 428x240 ms | GPU capture/upload/compute/fence 1920x1080 ms |
|---|---|---:|---:|---:|
| Vulkan | Nearest | 31.145 | 3.676 | 1.304 |
| Vulkan | Bilinear | 36.373 | 4.035 | 1.314 |
| D3D12 | Nearest | 31.458 | 3.666 | 1.376 |
| D3D12 | Bilinear | 36.201 | 4.031 | 1.319 |

These are isolated synthetic output costs, not gameplay frame rates or complete
frame budgets. Probe fences serialize work; normal rendering uses no fence or
readback. CPU fallback preserves the current native VRAM path; performance
profiling of actual game logic, pacing/waits and GPU work remains for the
follow-up runtime-fixes branch. Aggregate CPU/GPU usage alone is not a measure
of available game-thread headroom.

### Runtime sleep/save regressions (2026-10-26)

The Windows Application Error location for the owner's sleeping crash maps to
farm_action_pick_slot in the release executable. ASan/UBSan synthetic tests
reproduce the original truncated pointer scan and the old save packer's invalid
run16 read. Updated scans and fixed-wire codec pass their isolated regressions.
The codec test uses an in-memory card only. Interactive gameplay is owner-tested;
matching binaries are unverified because the Linux toolchain is unavailable.

### Runtime follow-up validation (2026-10-27)

Windows Release and ASan/UBSan builds pass 89/89 tests after dialogue argument,
overnight loader progress, runtime math and ordinary-card PocketStation handling
changes. No gameplay automation or performance claim accompanies these checks.
Interactive overnight/dialogue/Yodel Ranch validation remains with the owner.

### Second source-audit validation (2026-10-27)

Native Windows commands: cmake --build build/port/windows-release and ctest
--test-dir build/port/windows-release --output-on-failure; equivalent commands
for windows-asan-ubsan. Both builds and all 99 tests pass. A separate ASan family
name fixture reads the SHA-1-validated owner disc without executing gameplay.
Seven touched translation units pass targeted Clang analysis without selected
core/unix/security diagnostics. No performance improvement or other-device result
is claimed. Matching check is unavailable without the separate Linux toolchain.

### Third source-audit validation (2026-10-27)

Native Windows Release and ASan/UBSan builds and ctest --output-on-failure pass
106/106 tests each. Targeted Clang analysis of six changed game translation units
reports zero selected core/unix/security and callback/return/pointer-cast findings.
Two original fixtures fail before repair, and a separate boolean terrain fixture
reproduces a null store. These are synthetic technical checks only; gameplay,
other devices, matching output and performance remain unverified by this batch.

### Wide transition validation (2026-10-27)

Windows Release and ASan/UBSan builds and ctest --output-on-failure each pass
106/106 tests after ADR-068. Rebuilt hmbtn_gpu_renderer_probe and ran it with
direct3d12 and vulkan; native/enlarged world/UI, filtering, full-screen TILE
shade and presentation readbacks pass on both. Shaders were regenerated using
the pinned DXC script. No game/replay, owner save, performance benchmark or
other-platform build was run. Native scanout uses the same wide canvas when
command replay is unavailable; content remains proportional.

### Summary and splash follow-up (2026-10-27)

Release/ASan builds and ctest --output-on-failure pass 107/107 tests. The added
fixture executes the original farm-summary sprite loader with bounded synthetic
CD progress and validates five shared sprite slots. Rebuilt offscreen GPU probe
passes Vulkan and D3D12 readbacks with original white/black DRAWENV backgrounds,
both frame-buffer origins, native/enlarged output and filtering modes. Shader
headers are regenerated with pinned DXC. No performance claim or other-platform
build is made. Matching check cannot run without the separate Linux toolchain;
original status-page source alternatives reconstruct exactly with native guards
disabled. User gameplay validation remains separate from these fixtures.

### Fade/camera context regression checks (2026-10-27)

Windows Release and ASan/UBSan builds and ctest --output-on-failure pass 108/108
tests. enhance.fade_buffers executes original fade packets over both framebuffers
and compares the expanded software canvas to native scanout through 64 frames.
gfx.gpu_scene_packets reproduces/restores the captured background lifetime;
game.work_memory executes original reset code for saved/menu farm returns and
ordinary title/save scenes. All three regression cases fail before correction.
Rebuilt offscreen GPU probes pass on Vulkan and D3D12 with a changed live context
before scene finalization. No shaders, dependencies or matching sources change.
No benchmark, other-platform build or automated gameplay is claimed.

### Blocking-wait display coherence (2026-10-27)

Release and ASan/UBSan tests pass 109/109 after ADR-072. gfx.frame_coherence
validates unchanged scanout across a long wait and native/private transfer
invalidation, including wrapped uploads and direct loading OTs. Original fade
fixtures insert 120-tick waits during fade-out. Rebuilt GPU probes pass Vulkan
and D3D12 offscreen readbacks. No performance benchmark or other-platform build
was run. Release test binaries were rebuilt separately while the owner's game
held the main executable open. After the owner closed it, the full native Release
build linked the updated main executable successfully.

### Native farm re-entry validation (2026-10-27)

Windows Release and ASan/UBSan builds pass 110/110 tests after ADR-073. The farm
fixture mutates/restores all 677 mutable farm objects across repeated activations,
preserves resident state, checks const classification and read-only cue tables.
The original lifecycle test reproduces the prior re-entry abort without a disc.
Clang AST --check covers all 51 TUs and verifies the checked metadata; field/status
SDK definitions are retained by their generated wrappers. Normal builds need no
Python/AST generation. No other-platform, gameplay or performance claim is made.

A separate Windows build with all enhancement options OFF also compiles the farm
and lifecycle fixtures; both overlay.farm_reentry and overlay.title_load_return
pass. This is a targeted OFF-build check, not a full second test-suite run.

ADR-075 validation (2026-10-28): Windows Release and ASan/UBSan builds use 12 jobs;
both full suites pass 111/111 tests. The added fixture reproduces the former
pickup pointer overwrite, preserves four native actor prefixes and all seven
helper IDs, and exercises 16 young/adult dog pickup/drop combinations plus 960
phase updates. An enhancements-OFF build passes the actor-layout and farm/title
re-entry fixtures (3/3). Clang AST --check still audits all 51 farm units,
703 objects and 56 local cue tables. No assets, gameplay or real save files are
used by these fixtures. Matching source is unchanged; matching check and other
platform builds were not run on this Windows host.

### Full suite and Android platform validation (2026-10-30)

An Android GPU presentation retry fix retains cached scene data but submits the
cached image again when presentation is requested. An encode failure invalidates
the cached serial so the next attempt can rebuild it. This addresses a retry
failure mode; the reported gameplay black screen and device performance remain
unverified without the affected Android device.

- Windows Release build passes 122/122 unit tests (100% passing) across native PS1 emulation, GPU packets, GTE projection, card persistence, and enhanced gameplay modules.
- Packaged Windows standalone release output produced at dist/VanillaBTN-windows-x64-v0.9.0.zip containing VanillaBTN.exe and README.txt.
- Android platform build passes clean compilation under NDK r27c / Clang 18.0.3, generating signed release APK at dist/VanillaBTN-android-v0.9.0.apk (6.21 MB).
- Fast Text page break and dialog advancement verified cleanly under ADR-106 and ADR-109. Rucksack cross-category tool/item protection verified under ADR-107.

### Tile marker visibility and ground row gaps (ADR-111, 2026-10-30)

Windows Release `build.ps1 -Preset windows-release -Game` builds the game,
passes 124/124 tests, and produces the standalone v0.9.0 ZIP. The three focused
gameplay/interpolation tests also pass with the Windows ASan/UBSan preset.
Marker checks include actual opaque-terrain occlusion and the disabled path.

The synthetic offscreen `hmbtn_gpu_renderer_probe direct3d12 --tile-seams`
passes shared-row readback at 32x32, 65x97 and 1128x634 with 1/2/4 samples.
This reproduces mixed interpolation fallback between adjoining sprite rows;
it does not confirm the owner's complete gameplay scene or other GPUs.
The broader GPU reference probe separately fails its native color comparison
(GPU 130 versus CPU 132); its shader output adds micro-dithering while that
fixture expects an exact match. This task does not change shaders or relax
that comparison. No interactive gameplay, Android/device validation, other
platform builds or Linux matching check was run. Matching source is unchanged.

### Active primitive arena correction (ADR-112, 2026-10-30)

The owner's running Windows game confirmed the missing-marker cause: Tile
Targeting was ON in normal farm play, with only 23,704 bytes used in the active
private arena, but the indicator checked the unrelated overlay backup buffer.
The corrected capacity check covers Tile Targeting and the tool charge gauge.
Regression checks use separate active/backup allocations and verify that a
genuinely full active arena rejects both indicators.

The Windows Release game build and standalone package pass 124/124 tests;
the focused gameplay test also passes Windows ASan/UBSan. The owner requests
16 build jobs for this machine; Windows build.ps1 now uses `-j16`, and a
subsequent incremental `cmake --build build/port/windows-release -j16` passes
with no remaining work. The owner reopened the rebuilt game, and direct window
capture confirms a cyan tile marker in front of the player during normal farm
gameplay. This observation does not establish other-area coverage. Horizontal
lines while running are explicitly deferred; no additional graphics fix is
claimed. Matching source is unchanged and its Linux check was not run.

The shared GPU output no longer adds unconditional presentation noise after
BGR555 colour expansion (ADR-113). Original primitive dithering is unchanged.
Owner gameplay validation is pending; GPU/CPU parity has not been remeasured.

GPU packets that interpolate motion use affine texture mapping when PGXP is
also enabled; non-interpolated PGXP packets retain perspective correction
(ADR-114). Owner visual validation is pending.
