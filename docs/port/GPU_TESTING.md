# Windows SDL_GPU presentation spike

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
blit, readback and presentation. D3D12, Metal, other devices, suspend/resume and
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
A/S are L1/R1, Q/E are L2/R2 and Backspace is Select. Free control and physical
keyboard input remain under validation; save persistence is not implemented.

The headless boot probe also accepts HMBTN_CAPTURE_FRAME to export its final
15-bit scanout to BMP at HMBTN_BOOT_TICKS, and HMBTN_TRACE_GAME_STATE to print
area/player state every 600 ticks. These diagnostics require the owner's disc
for game frames. Store captures only in ignored build output. A 6000-tick
Vulkan/ASan window capture shows farm terrain/player/HUD; a release headless
capture shows the original grandparent dialogue. Later opening scenes still
need fixes before this is a playable port.
