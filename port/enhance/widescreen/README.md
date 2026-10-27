# Widescreen world view

HMBTN_ENH_WIDESCREEN gates this module (CMake default OFF). Windows -Game
compiles it; enhance.widescreen defaults false. ESC > Settings > Graphics >
Widescreen Mode toggles it. Source game data and assets are unchanged.

The private render area grows horizontally around the original center; original
GTE projection/depth and camera eye/target stay intact. Model frustum/clip bands,
field grid coverage and projected sprite visibility expand to reveal additional
world. World OT clipping expands; normal UI retains its original size and stays
centered. Full-screen flat F4 and variable TILE fades/fills cover the sides too. Side initialization
uses the native DRAWENV clear colour when enabled, otherwise black, never
adjacent texture/CLUT VRAM. Texture feedback still checks native
framebuffer coordinates. Shared capture and interpolation preserve OT groups.

Works through GPU Auto or the software reference, with scaling either ON or OFF.
16:9 uses an even 428x240 logical domain; actual enhanced output uses window pixel
size. This rounding gives a small subpixel aspect approximation at native height.
21:9/32:9 use the same calculation. Maximum logical width is 1024. Portrait/4:3
remain native. OFF retains 4:3 bars and original source culling.

Widescreen presentation now applies from the first logo through title, load/save,
minigames, gameplay and pause (ADR-057). The title's original scrolling 0x25
background tiles are tagged separately and exposed beyond their old clip;
logo/text/cursor packets stay proportional. Native/direct/loading scanout stays
centred on the expanded canvas when command replay is unavailable. Uncovered
pixels use the native clear colour; no source edge columns are stretched into invented background.
Original black logo background remains black. Nothing invents new level scenery
or enlarges weather particle pools. Scene-specific geometry/coverage still needs
owner testing. GPU/native fallback never changes the presentation aspect.
Original primitive memory/overlay backups stay intact. Model submission stops
with an 8 KiB UI reserve if its original arena fills. Two optional 256 KiB GS
arenas supply sprite packet space; OFF restores original work arenas.
CPU fallback at large output sizes can be slow.

ADR-068 captures direct frame-blend OTs even when the game skips PutDrawEnv.
Their commands accumulate over the captured scene, including its extended sides,
until a normal frame replaces it. Consecutive equal OT groups are coalesced;
capacity exhaustion invalidates replay instead of using partial geometry.
The 640x480 stepped-fade domain also expands (856x480 at 16:9).
Rucksack/name-entry TILE shade preserves its three-word command boundary.
ADR-070 makes GPU/software/native side initialization follow the actual clear
colour (including the white Natsume splash), independent of changing edge texels.
Output aspect stays wide even for direct/loading scanouts, while
native artwork remains proportional. This is a presentation guarantee in the
implemented paths, not proof that every map has sufficient original side assets.

Technical checks cover world/UI separation, full-screen fills, unchanged native
VRAM/environment/ticks/projection, OFF culling, GS cursor/arena restoration and
GPU readback on Vulkan/D3D12 at native/enlarged output with both filtering modes.
No gameplay or replay was launched. Owner validation across maps, weather,
transitions, dense fields, dialogue, interpolation and monitor changes is pending.
See ADR-056, ANALYSIS.md and PLATFORMS.md.

GPU Auto uses native-resolution retry for unsupported scenes to avoid a large CPU
raster stall; explicit Software retains viewport-sized reference rendering.
Supported GPU scenes still use Enhance Scaling's requested size. A retry can
reduce detail temporarily. See PLATFORMS.md for synthetic dialogue measurements.

ADR-072 keeps completed displayed frames across blocking CD/audio waits without
a tick-age timeout. Native framebuffer image writes, direct loading OTs and
reset invalidate the affected captures; unrelated texture uploads and private
rendering do not. This supersedes earlier age-based rejection and avoids losing
the extended canvas merely because simulation waits on assets. Actual reported
Natsume/TV/home-exit transitions remain owner-tested.
