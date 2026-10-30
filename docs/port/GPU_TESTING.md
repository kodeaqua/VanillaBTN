# Windows SDL_GPU presentation spike

## Current game renderer status

This file documents synthetic GPU checks, not a claim of complete game-scene
coverage. The Windows game uses shared GPU scene output at native and enhanced
resolutions and retains Software as a reference. Captured render-producing GP0
commands are represented on the compute path: polygons, lines, rectangles,
clear-image, load-image, VRAM-to-VRAM copies, and ordered VRAM feedback. Mask-set
and mask-check behavior is preserved for load and VRAM-copy transfers. VRAM-to-CPU
readback, IRQ requests, NOP/cache commands, and draw-environment state do not
produce scene pixels. Malformed streams, missing/direct captures, reserved texture
depth, and GPU resource/device failures still use the reference path.
Gameplay parity, device breadth, and frame-time budgets remain owner checks; see
PLATFORMS.md and ENHANCEMENTS.md.

The SDL-enabled build provides `hmbtn_gpu_renderer_probe`, an offscreen GPU
readback comparison against the CPU reference. It covers the primitive classes,
texture depths and blend modes, clear/load/copy ordering, wrapped and overlapping
VRAM copies, mask-set/check transfers, VRAM texture feedback,
high-density tile bins, scene payloads above the former fixed buffer size,
clipping, filtering, interpolation, multisampling, and expanded presentation.
It starts no game window and requires no disc. On 2026-10-31, the expanded probe
passed with SDL GPU's Vulkan and Direct3D 12 drivers. This is technical coverage
of synthetic scenes only; owner gameplay testing remains necessary.

The core `gpu.vram` test also checks overlapping `MoveImage` copies, including
wrap at the VRAM edge and mask-bit behavior. These transfers now snapshot their
source before writing, so overlapping areas do not terminate the game.

Android's SDL GPU device uses SPIR-V only and disables optional Vulkan features
unused by this compute renderer. The connected MediaTek G100 tablet reports a
Mali-G57 MC2 Vulkan device with `shaderClipDistance` unavailable; SDL 3.2.28
rejected it before device creation. SDL 3.4.18 exposes the optional-feature
properties needed to accept that device. After updating the SDL Java glue to
3.4.18 as well, the rebuilt APK passed JNI startup and logged `GPU Device
created and claimed successfully (Driver: vulkan)` on 2026-10-31. The app was
stopped immediately after GPU initialization; rendering and gameplay were not
tested on the tablet.

This optional local diagnostic validates GPU presentation without game data.
It is not a game screen or a primitive rasterizer. Default unit builds neither
download SDL nor require a GPU. The SDL release and SHA-256 are pinned in the
port CMake project; see THIRD_PARTY.md.

From the repository root, use a PowerShell session with the local tools:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass
& ./port/packaging/windows/setup-toolchain.ps1
cmake -S port -B build/port/windows-gpu-spike -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DHMBTN_BUILD_GPU_SPIKE=ON
cmake --build build/port/windows-gpu-spike --target hmbtn_gpu_spike -j 4
& ./build/port/windows-gpu-spike/hmbtn_gpu_spike.exe
```

The program uploads a synthetic RGBA gradient/checker pattern, performs a
nearest GPU blit into a second texture, verifies byte-exact CPU readback, then
presents eight frames in a window named HMBTN native graphics validation. It
limits frames in flight to two and closes automatically. No assets are read,
no screenshot is saved, and no config/save files are written.

Validated on 2026-10-25 with SDL3 3.2.28, Windows x86-64, Radeon RX Vega and
driver 31.0.21925.1001. Output reported the Vulkan backend and successful upload,
blit, readback and presentation. The offscreen scene probe separately passed on
Vulkan and Direct3D 12 on 2026-10-31. Metal, other devices, suspend/resume and
game-frame performance remain untested. This synthetic diagnostic stays separate
from the game-window target. See ADR-018 for scope and backend status.

The optional HMBTN_BUILD_NATIVE_WINDOW=ON target hmbtn_native links the compiled
game, reference DrawOTag rasterizer and SDL_GPU scanout. It accepts the original
USA BIN argument described in DISC_TESTING.md. Local Vulkan captures now show
the title/menu and initial New Game dialogue. A 1200-VBlank ASan/UBSan window
run also exercised the SDL audio callback. No full-game, golden-frame parity or
frame-time/latency budget is established. ASan and reference CPU rasterization
are expected to affect pacing; observed audio underflows remain an open issue.

Build the Windows release game window (also runs the unit suite):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/run.ps1 -DiscPath 'D:/Games/Harvest Moon USA.bin'
```

The launcher builds the release window if missing and clears diagnostic input
replays/tick limits for the child game, restoring the caller's environment on
exit. It does not copy disc data into the repository. Keyboard: arrows move,
Enter is Start, X is Cross/confirm, Z is Square, D is Circle, W is Triangle,
A/S are L1/R1, Q/E are L2/R2 and Backspace is Select. A 31000-tick replay
validates free movement in the house (WINDOWS_PLAYTEST.md). Physical keyboard
input remains unverified by automation. Native file-backed card containers are
implemented; the complete in-game sleep/save/load lifecycle remains owner-unverified.

The headless boot probe also accepts HMBTN_CAPTURE_FRAME to export its final
15-bit scanout to BMP at HMBTN_BOOT_TICKS, and HMBTN_TRACE_GAME_STATE to print
area/player state every 600 ticks. These diagnostics require the owner's disc
for game frames. Store captures only in ignored build output. A 6000-tick
Vulkan/ASan window capture shows farm terrain/player/HUD; a release headless
capture shows the original grandparent dialogue. Longer headless replays now
complete the opening and house movement; broader gameplay remains under test.
