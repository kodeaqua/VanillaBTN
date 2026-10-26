# Enhancements

The owner requested the native pause interface and graphics settings on
2026-10-26. Implementation starts on port/windows-pause-menu, after PR #1
(windows port init) was merged and all old branches were removed.

| Feature | Current implementation |
| --- | --- |
| ESC pause, Resume, Settings > Graphics, Exit Game | Host interface implemented; owner interactive test pending |
| Original dialog presentation/font | Original window-frame C and runtime disc glyph cache; synthetic composition and local asset decoding checks pass |
| Monitor/window mode/resolution | SDL display controls implemented; owner monitor/fullscreen test pending |
| Filtering | Nearest/bilinear final-frame presentation; per-texture filtering is pending |
| VSync | On (mailbox/FIFO) or supported immediate Off; device validation pending |
| FPS limit | Native (30) or monitor-derived rational caps; fixed 60 Hz game ticks; native images unless optional interpolation is enabled |
| Visual interpolation | Optional private primitive replay for normal farm views; synthetic checks pass, owner gameplay validation pending |
| Enhance Scaling | OFF; native higher-resolution scene renderer pending |
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
port/enhance/interpolation/README.md. Scaling, widescreen ON and AA remain pending.
