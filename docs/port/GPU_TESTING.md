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
