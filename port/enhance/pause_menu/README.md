# Native pause interface

Initial implementation on port/windows-pause-menu, based on merged main.
HMBTN_ENH_PAUSE_MENU defaults OFF in CMake. Windows build.ps1 -Game enables it
for the owner's explicitly requested ESC interface. Set enhance.pause_menu to
false in settings.json to disable the runtime interface; existing settings
without that key use the requested available-by-default behavior.

The platform pauses at its VBlank host boundary without advancing game/CD/SPU
ticks. The private UI loop still pumps window events and presents frames.
Audio device playback pauses and the pacing epoch shifts by the pause duration.
Held menu-confirm input is suppressed until release on returning to gameplay.

The font is read from the validated owner's disc, using the game's original
resident UI texture-bank decoder, original English glyph layout and text CLUT.
No game pixels are stored in the repository. A private cache survives overlays.
The dialog frame executes src/main/ui/window_frame.c with private pixel/line/
gradient sinks. No game frame buffers, ordering tables or dialog state are used
while the pause interface is open. No replacement bitmap/font is authored.
Frame color arguments follow prim_make_pixel/line/gouraud_rect's 0xRRGGBB
contract. An inaccurate older comment in window_frame.c must not be used to
reverse the red and blue channels. The private canvas keeps the original cream
palette, with compact centered root/settings panels and a separate Graphics
header, aligned label/value columns, selected-row frame and detail footer.

Controls: ESC opens the root menu or backs out one level; ESC at the root resumes.
Arrows navigate/change options; Enter or X confirms; D goes back. Exit Game
closes the application. The requested tree is Resume, Settings > Graphics,
Exit Game. Root and settings have no extra entries.

Implemented platform controls: monitor selection; Windowed, exclusive Fullscreen
and Borderless Fullscreen; resolutions enumerated from SDL's driver display
modes; nearest or bilinear final-frame presentation. Borderless follows the
desktop resolution. These display operations require the owner's hardware test.
Normal presentation preserves 4:3 with black bars. Bilinear currently filters
the final framebuffer, not each original primitive's texture samples.

VSync offers synchronized On (mailbox/FIFO) or supported immediate Off. FPS
Limit offers Native (30) and monitor refresh-derived rates, preserving rational
values such as 60000/1001 (shown as 59.94). Windowed/borderless choices use the
desktop resolution; exclusive choices use the selected resolution. VSync can
reduce achieved presentation to the active OS refresh. A cap does not switch
desktop refresh. The nine-item list scrolls through seven visible rows.

Presentation deadlines are independent of the fixed 60 Hz hardware ticks.
Original normal game updates remain 30 Hz through VSync(2). Higher caps repeat
native pictures, without advancing game/RNG/audio faster. Interpolated motion
is not implemented. GPU/OS stalls and achieved FPS remain owner hardware tests.

The Graphics menu lists Enhance Scaling, Widescreen Mode and Anti-Aliasing as
OFF with renderer support pending. They cannot be enabled yet. Native higher
resolution scene rendering, extended camera/frustum/culling and FXAA/MSAA need
separate renderer implementations; neither stretching nor final-frame filtering
is represented as those features.

The graphics object in Documents/BackToNature/settings.json stores monitor
ordinal, window_mode (0 windowed, 1 exclusive, 2 borderless), width, height and
bilinear (0 or 1), vsync (0 or 1), fps_numerator and fps_denominator. Zero FPS
numerator selects Native. Defaults are VSync On and Native (30). Unknown root properties are preserved when replacing the
owned graphics object. Malformed JSON is not overwritten. Writes use the
existing temporary-file replacement and instance lock. Missing or invalid
graphics fields fall back to windowed 640x480, nearest filtering, monitor 0.

Tests use synthetic glyph data for menu geometry/navigation, JSON validation and
unknown-property preservation. A separate local asset probe decodes the real
font and checks VRAM is reset, without opening a window or starting the game.
Presentation-clock tests verify exact rational boundaries and unchanged native
VBlank callback counts across caps; these do not run the actual game clock.
Physical ESC behavior, audio pause/resume and display switching remain owner
gameplay tests. No interactive session or replay is run by the agent.
