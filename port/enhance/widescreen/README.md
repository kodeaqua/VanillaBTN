# Widescreen world view

HMBTN_ENH_WIDESCREEN gates this module (CMake default OFF). Windows -Game
compiles it; enhance.widescreen defaults false. ESC > Settings > Graphics >
Widescreen Mode toggles it. Source game data and assets are unchanged.

The private render area grows horizontally around the original center; original
GTE projection/depth and camera eye/target stay intact. Model frustum/clip bands,
field grid coverage and projected sprite visibility expand to reveal additional
world. World OT clipping expands; normal UI retains its original size and stays
centered. Full-screen flat F4 fades/fills cover the sides too. Side initialization
is black, never adjacent texture/CLUT VRAM. Texture feedback still checks native
framebuffer coordinates. Shared capture and interpolation preserve OT groups.

Works through GPU Auto or the software reference, with scaling either ON or OFF.
16:9 uses an even 428x240 logical domain; actual enhanced output uses window pixel
size. This rounding gives a small subpixel aspect approximation at native height.
21:9/32:9 use the same calculation. Maximum logical width is 1024. Portrait/4:3
remain native. OFF retains 4:3 bars and original source culling.

The current scope is the farm normal-view world renderer, including its UI
layers. Title, menu-only scenes, minigames, direct/loading scanouts and the ESC
pause canvas retain 4:3. Authored scene edges can expose empty space; this feature
does not create level geometry or enlarge weather particle pools. Further actor
or scene-specific culling may still need adjustment after owner gameplay checks.
Original primitive memory/overlay backups stay intact. Model submission stops
with an 8 KiB UI reserve if its original arena fills. Two optional 256 KiB GS
arenas supply sprite packet space; OFF/scene exit restores original work arenas.
CPU fallback at large output sizes can be slow.

Technical checks cover world/UI separation, full-screen fills, unchanged native
VRAM/environment/ticks/projection, OFF culling, GS cursor/arena restoration and
GPU readback on Vulkan/D3D12 at native/enlarged output with both filtering modes.
No gameplay or replay was launched. Owner validation across maps, weather,
transitions, dense fields, dialogue, interpolation and monitor changes is pending.
See ADR-056, ANALYSIS.md and PLATFORMS.md.
