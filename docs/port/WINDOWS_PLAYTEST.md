# Windows native playtest

This is a development build of the original compiled game C. It can complete
the opening, exit the house, decline the tour, finish Zack's introduction and
respond to free player movement in the farm. Full playthrough,
save persistence, overlay reentry and hardware parity are not established.
Use the owner's original USA SLUS_011.15 BIN; it is validated at startup and
is never copied into git. No WSL, Docker or CI is required.

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
powershell -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/run.ps1 -DiscPath 'D:/Games/Harvest Moon USA.bin'
```

The executable is build/port/windows-release/hmbtn_native.exe. The launcher
builds it if missing, disables diagnostic replay/tick limits for normal play,
and restores the caller's environment afterward. Click the game window for
keyboard focus. Enter is Start; use arrows to select New Game and X to confirm.
X selects name characters and Enter accepts a completed name. Progress opening
dialogue with X.

| Keyboard | Original pad |
| --- | --- |
| Arrows | D-pad |
| X / Z | Cross / Square |
| D / W | Circle / Triangle |
| A / S | L1 / R1 |
| Q / E | L2 / R2 |
| Enter / Backspace | Start / Select |

## Verified checkpoint

On 2026-10-26 the authored input file
port/tests/replays/new_game_control.replay completed 31000 VBlanks in Windows
release and optimized ASan/UBSan headless builds with the owner's validated
disc. It traversed the grandparent/flashback scenes, XA playback requests and
the morning house. After the intro script released control, right/down input
changed player coordinates from (-90, 0, 541) at tick 30000 to
(1058, 0, -1386) at tick 30600. Both runs exited zero. Their final BMP captures
have identical SHA-256:

808c2ce6346e216d6792297f6d5d919e49d4e60328065034e0abe7f66d4dd37b

This compares these two native runs, not an original-hardware golden frame.

The longer new_game_farm.replay completes 39000 VBlanks in release and optimized
ASan/UBSan, including house exit, declining Thomas's tour and Zack's shipping
dialogue. With scripting/paused state cleared, down input moves the player
from (-4257, 0, 3959) to (-4257, 0, 1663); right input then moves him to
(-1838, 0, 1663). The final scene is the farm, with its HUD showing 7:00 AM.
Both runs exit zero and their final BMP SHA-256 is:

dbe06cf4b46a8cce8e2814f7f03af3058e1f20cd4a84b5462b3ceb321905d044
Physical keyboard focus/input was not automated successfully; the replay feeds
the same native pad samples used by the window host. A separate Vulkan window
test completed 6000 VBlanks and rendered farm terrain, player and HUD. No
glitch-free listening, frame-time budget or full-game claim follows from these
checks. Audio underflows were observed in instrumented window tests.

For local reproduction, build hmbtn_boot_probe in the desired build directory,
set HMBTN_INPUT_REPLAY to the authored replay, HMBTN_BOOT_TICKS=31000,
HMBTN_TRACE_GAME_STATE=1 and HMBTN_CAPTURE_FRAME to an ignored output BMP, then
run the probe with the validated BIN path. Unit tests need no disc. Never add
the disc, screenshots or other extracted data to git.

Matching check remains unverified because this Windows host lacks the separate
Linux toolchain and the owner excludes WSL. Source-branch reconstruction is a
separate textual check and does not establish matching compiler output.
