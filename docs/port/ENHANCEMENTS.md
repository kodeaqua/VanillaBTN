# Enhancements

The owner requested the native pause interface and graphics settings on
2026-10-26. Implementation starts on port/windows-pause-menu, after PR #1
(windows port init) was merged and all old branches were removed.

| Feature | Current implementation |
| --- | --- |
| ESC pause, Resume, Settings > Graphics, Exit Game | Host interface implemented; owner interactive test pending |
| Original dialog presentation/font | Original window-frame C and runtime disc glyph cache; synthetic composition and local asset decoding checks pass |
| Monitor/window mode/resolution | SDL display controls implemented; owner monitor/fullscreen test pending |
| Filtering | Optional palette-aware texture bilinear plus final-frame presentation (ADR-055) |
| VSync | On (mailbox/FIFO) or supported immediate Off; device validation pending |
| FPS limit | Native (30), Off (Unlimited), or monitor-derived rational caps; fixed 60 Hz game ticks; native images unless optional interpolation is enabled |
| Visual interpolation | Optional private primitive replay for normal farm views; synthetic checks pass, owner gameplay validation pending |
| Enhance Scaling | Optional viewport-resolution output through GPU Auto or Software; default OFF; gameplay checks pending |
| Widescreen Mode | OFF path uses centered 4:3 and black bars; expanded camera/culling ON path pending |
| FXAA/MSAA | OFF; renderer paths pending |

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
port/enhance/interpolation/README.md. Widescreen ON and AA remain pending.

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
