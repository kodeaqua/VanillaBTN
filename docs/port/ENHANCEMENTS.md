# Enhancements

ADR-124 defines Graphics Mode: Native PS1 (default), Enhance PS1 (PGXP and
dithering bypass), and Modern (the same fixes plus individually selectable
visual effects). Modern choices are retained when another mode is selected.
Existing saved enhancement choices are migrated. GPU/CPU coverage and visual
results remain subject to owner gameplay validation. On Android the existing
compact menu exposes only the mode selector; individual Modern controls remain
desktop-only, and mobile interpolation stays disabled. Earlier menu descriptions
below are historical and are superseded by ADR-124.

Disabling dithering removes the ordered pattern, but the current shared GPU
scene still quantizes intermediate colors to BGR555. ADR-125/127 preserve 8-bit
shading for opaque untextured Gouraud and non-raw, unfiltered textured triangles
in enhanced GPU output. Other paths and software fallback still use BGR555.
This reduces banding only in those cases. Older claims below and in ADR-084/092
are superseded by this limitation and ADR-113/125/127.

The owner requested the native pause interface and graphics settings on
2026-10-26. Implementation starts on port/windows-pause-menu, after PR #1
(windows port init) was merged and all old branches were removed.

| Feature | Current implementation |
| --- | --- |
| ESC pause, Resume, Settings > Graphics, Exit Game | Host interface implemented; owner interactive test pending |
| Original dialog presentation/font | Original window-frame C and runtime disc glyph cache; synthetic composition and local asset decoding checks pass |
| Monitor/window mode/resolution | SDL display controls implemented; owner monitor/fullscreen test pending |
| Filtering | Texture filtering: Nearest Neighbor, Bilinear, and xBR sharp edge-directed curves (ADR-085) |
| VSync | On (mailbox/FIFO) or supported immediate Off; device validation pending |
| FPS limit | Native (30), Off (Unlimited), or monitor-derived rational caps; fixed 60 Hz game ticks; native images unless optional interpolation is enabled |
| Visual interpolation | Optional private primitive replay for normal farm views; synthetic checks pass, owner gameplay validation pending |
| Enhance Scaling | Optional viewport-resolution output through GPU Auto or Software; default OFF; gameplay checks pending |
| Widescreen Mode | All-screen wide canvas, original title background tiles, proportional UI and stable fallback (ADR-057); owner validation pending |
| FXAA/MSAA | FXAA (final-image) and MSAA 2x/4x (GPU scene) implemented (ADR-072); runtime default Off; owner gameplay validation pending |
| Gameplay Enhancements | Suite of 9 toggleable options in Settings > Gameplay (ADR-080); runtime default OFF except wild dog; owner gameplay validation pending |
| Controls Settings | Gamepad, keyboard presets, and mouse interaction in Settings > Controls (ADR-081); persistent in settings.json |
| PGXP Geometry | Subpixel vertex precision and perspective-correct texture mapping (ADR-083); reduces wobble and texture warping where supported; default OFF |
| Dithering | PS1 hardware 4x4 Bayer ordered dithering toggle (ADR-084); original PS1 dither pattern or bypass; native mode retains dithering |

HMBTN_ENH_PAUSE_MENU defaults OFF in CMake; Windows build.ps1 -Game enables it.
The owner explicitly requests ESC availability across scenes, so that interface
defaults available at runtime; enhance.pause_menu=false disables it. This is
recorded as a narrow owner override in AGENTS.md. Rendering enhancements retain
their OFF defaults and cannot be enabled through placeholder controls.

Graphics properties are saved in Documents/BackToNature/settings.json. Unknown
root properties are preserved; malformed input is not overwritten. No game
state, RNG, CD progress, GPU VRAM or SPU synthesis advances inside the pause UI
loop. Paused wall time is excluded from the pacing epoch. No game art, fonts,
SDK material or BIOS is included in the repository. See ADR-048 and
port/enhance/pause_menu/README.md for controls, schema and current limits.
VSync/FPS controls follow ADR-049. Presentation counts do not advance game time.
Achieved FPS, physical VSync and gameplay time remain owner tests.

The owner subsequently requested visual interpolation (ADR-050). Windows -Game
builds HMBTN_ENH_INTERPOLATION; runtime enhance.interpolation defaults false.
Enable Interpolation and choose a higher FPS Limit in Graphics. Positions are
interpolated through private GPU command replay, without extra game updates or
image crossfading. Initial scope is normal farm views; unmatched primitives,
loading, transitions and other contexts retain native cadence. See
port/enhance/interpolation/README.md. Optional farm Widescreen ON is available (ADR-056); AA remains pending.

FPS Limit Off removes the software presentation cap (ADR-051). VSync On still
synchronizes to the active monitor; Native remains the default. A synthetic
regression verifies 600000 visual attempts do not add hardware/game ticks.
The pause backdrop also receives a 40% black overlay; original dialog/text
remain fully bright, and native VRAM is unchanged.

Enhance Scaling now has a private viewport-resolution raster path (ADR-052).
Windows -Game compiles HMBTN_ENH_SCALING; enhance.scaling defaults false. The
initial implementation rerasterizes triangles/quads at viewport size, preserving
original texture assets and 4:3 bars. Output supports up to 4096 per axis with software fallback. High FPS in actual gameplay is not established. See
port/enhance/scaling/README.md and the synthetic measurements in PLATFORMS.md.

Interpolation now uses stable field-cell identities and a shared-vertex mesh
coherence guard (ADR-053). The owner confirmed the reported ground cuts disappear
with Interpolation OFF. Synthetic reproductions for mismatched neighboring
history and reordered field packets pass after the fix; owner farm confirmation
is still pending. History changes may reduce interpolation cadence locally.

Renderer > GPU Auto is now shared by native and enhanced output (ADR-054),
independent of enhance.scaling. Software remains selectable. Native GPU pixel
comparisons pass on Vulkan/D3D12; unsupported transfers/feedback and direct
loading retain CPU fallback. The normal low-resolution VRAM compatibility path
and pause canvas still use CPU. No complete GPU-offload or playthrough claim.

Texture Bilinear now works in GPU Auto and Software, at native or enhanced
resolution. HMBTN_ENH_FILTERING gates compilation; enhance.filtering defaults
false, with migration of an existing explicit graphics.bilinear setting.
Nearest preserves the reference path. Transparent taps preserve the background;
UV bounds prevent neighboring atlas samples. See enhance/filtering/README.md
and ADR-055 for mask policy and limitations. Actual gameplay remains unverified.

Widescreen (ADR-056) now expands horizontal world coverage on the farm normal
view, including its UI overlays. HMBTN_ENH_WIDESCREEN gates compilation;
enhance.widescreen defaults false. Windows -Game includes it. OFF and non-world
scenes retain 4:3 bars. GPU Auto/Software, native/enhanced output and palette
filtering share the path. Original UI size, GTE projection and game clocks stay
native. See port/enhance/widescreen/README.md for current limits and owner checks.

ADR-057 supersedes the world-only restriction above: Widescreen now applies to
logo/title/load/save/minigames/pause as well as world rendering, with native
logos/text and background extension from original assets. GPU/native fallback
keeps the presentation aspect. Auto retries at native internal resolution to
bound stalls; explicit Software keeps full-resolution CPU output. Actual
in-game dialogue performance and all-screen visuals remain owner-tested.

ADR-068 fixes full-width rucksack TILE shade and captures direct frame-blend
commands over the retained extended scene. Native/direct/loading output uses a
centred scanout on the same expanded canvas when replay is unavailable; it does
not change back to a 4:3 destination. Uncovered pixels are black. Both renderers
stop seeding side pixels from changing native edge columns during logo fades.
This supersedes the earlier edge-extension description. Original artwork and
text remain proportional. Interactive all-screen coverage is still owner-tested.

ADR-070 supersedes the unconditional black backdrop: Natsume's original enabled
DRAWENV white clear now covers the expanded area too; disabled clearing uses
black. Shared frame replay keeps the capture for the actual displayed buffer
when simulation state advances before a flip. Interpolation still rejects
cross-scene blending. Owner verification of the reported Won flash is pending.

ADR-071 keeps GPU backdrop colour in the captured scene through finalization
and accepts completed clear-only OTs. Original fade packets are tested across
both framebuffers and changing backdrops. A separate native SDK adapter restores
the farm projection centre after save/menu returns, independently of enhancement
settings. Natsume/TV side flashes and persistent post-save framing remain pending
owner revalidation despite passing isolated regressions.

ADR-081 adds a dedicated Settings > Controls menu with Gamepad, Keyboard layout presets (Arrows+ZX / WASD+JK / Off), and Mouse modes (Off / Action / Move & Act with click-to-walk). Input is mapped cleanly to PS1 pad bitmasks and persisted under controls in settings.json. Unit tests cover configuration and pause menu transitions.

The owner requested PGXP subpixel vertex precision and perspective-correct texture mapping (ADR-083).
Windows -Game builds HMBTN_ENH_PGXP; runtime enhance.pgxp defaults false (vanilla parity).
When enabled, GTE projection captures high-precision 32-bit floating-point screen coordinates and
view-space depth W without modifying the integer coordinates used by original game logic or collision.
The GPU scene backend retrieves subpixel coordinates and depth values, rendering geometry with
perspective-divided texture coordinates ($u/w, v/w, 1/w$), reducing vertex snapping and affine texture distortion where supported. Graphics Mode controls this via Enhance PS1 and Modern.

The owner requested an option to toggle PS1 hardware dithering (ADR-084).
Windows -Game builds HMBTN_ENH_DITHERING; runtime enhance.disable_dithering defaults false (vanilla parity).
PS1 hardware uses 16-bit color depth (R5G5B5) and applies an ordered 4x4 Bayer dithering matrix to reduce color banding.
On modern high-resolution displays, this ordered pattern manifests as checkerboard noise.
Graphics Mode bypasses dithering in Enhance PS1 and Modern. Intermediate GPU colors remain BGR555.

The owner requested edge-directed sharp texture filtering without blur (ADR-085).
Filtering now supports three selectable modes in Pause Menu > Settings > Graphics:
- **Nearest Neighbor**: Authentic raw PS1 pixels.
- **Bilinear**: Soft/smooth linear interpolation.
- **xBR**: Edge-directed smart scaling that rounds pixel art corners into smooth diagonal contours while preserving 100% crispness without blur.

Additionally, FXAA is now bypassed during Pause Menu presentation to keep UI text, dialogue, and font pixel art perfectly crisp.
Graphics Mode supersedes the earlier individual PGXP and dithering controls (ADR-124).

## Recent Enhancements & Fixes (2026-10-29 to 2026-10-30)

### Fast Text Dialogue Flow (ADR-106 & ADR-109)
Fast Text provides instant dialogue text typing while strictly preserving game control codes and timing events:
- At TEXT_CODE_PAGE_MARK and TEXT_CODE_PAGE_END, Fast Text immediately pauses text advancement and locks 	ext_phase = TEXT_STATE_PAGE_BREAK.
- Prevents subsequent glyph evaluations from overwriting the phase back to TEXT_STATE_PRINTING.
- Displays the red blinking advance prompt at page breaks, ensuring player Confirm (X / Circle) presses cleanly step pages or open choices without dialogue freezing.
- When Fast Text is disabled (0), original vanilla PS1 typing speed and behavior execute identically.

### Rucksack Tool/Item Category Protection (ADR-107)
Enforces authentic inventory categorization in the rucksack reorganize screen:
- Disallows cross-swapping between Tool slots (left column) and Item slots (right column) under mouse clicks and controller navigation.
- Disallows splitting stacked items into empty tool slots.
- Restricts mouse hover cursor tracking to the marked category until a swap is confirmed or cancelled.

### Android Platform Optimization & 720p Resolution Cap (ADR-105)
- Enhance Scaling on Android is capped at a maximum internal resolution of 720p (1280x720) to balance visual crispness with battery life, thermal throttling, and sustained 60 FPS performance on mobile GPUs.
- Android builds feature touch controls, Storage Access Framework (SAF) disc selection, and automatic audio device management.

### Tile Targeting visibility and ground row seams (ADR-111)

Tile Targeting now rejects event/script control and paused, menu, transition,
riding, airborne and sleeping states. Its marker is ordered in front of the
terrain rather than hidden behind it. With interpolation enabled, adjoining
ground rectangles now share the existing polygon coherence/fallback rule,
preventing mixed history from opening horizontal gaps in enlarged output.
Defaults and the vanilla OFF paths are unchanged. Synthetic regression checks
do not replace the owner's gameplay validation of the reported screen lines.

ADR-112 corrects an additional missing-indicator cause observed in the owner's
running game: widescreen uses a private primitive arena, but Tile Targeting and
the tool charge gauge checked capacity against the original backup buffer.
Both now validate the active arena and reject draws only when its remaining
space is insufficient. The owner's horizontal-line report while running is
deferred; no further fix for that report is claimed here.

The audio resampler defaults to PS1 Gaussian. Studio HQ remains an explicit
opt-in through audio settings; existing saved choices are preserved (ADR-110).
