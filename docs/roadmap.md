# Native Windows roadmap

## Target

Bring up an **x64 native Windows executable** of the USA PS1 game before adding
enhancements. Keep the original gameplay, timing, internal display modes,
aspect ratio, texture behavior, audio, and save data. No high-resolution
rendering, widescreen, PGXP-style geometry fixes, filters, replacement assets,
or balance changes in this first port. Scaling the original pixels to a
window is presentation only; it must not change the internal render size.

The current source is a matching PS1 build, not Windows-ready C: it calls
PSY-Q directly, loads overlays into fixed MIPS addresses, and uses 32-bit
layout assumptions. The native port therefore needs a separate host target
and explicit compatibility boundaries. Keep the PS1 build and its SHA-1 checks
unchanged.

## Initial stack

| Concern | Choice for first bringup | Why |
| --- | --- | --- |
| Compiler and build | MSVC, C17, CMake presets, x64; `clang-cl` as an optional second compiler | The game source is C. Visual Studio's Windows C17 toolchain and CMake support avoid WSL, MSYS, and Docker. [MSVC C17](https://learn.microsoft.com/en-us/cpp/overview/install-c17-support?view=msvc-170), [CMake in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170). |
| Window and display | Pinned SDL3, a streaming texture, nearest-neighbor presentation | SDL3 works with MSVC/CMake. It presents original-size pixels in a Windows window without defining gameplay graphics. [SDL3 CMake](https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md), [streaming texture](https://wiki.libsdl.org/SDL3/SDL_LockTexture), [logical presentation](https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation). |
| PS1 graphics | Small software GPU/GTE compatibility layer, then SDL3 presents its pixels | The code builds PS1 ordering-table primitives, VRAM uploads, CLUT textures, and fixed-point geometry. A software path makes those rules explicit and testable; measure performance before considering a GPU backend. |
| Input | SDL3 gamepad and keyboard mapped to the existing PS1 pad state; rumble where supported | Keep the original button positions, polling cadence, repeat rules, and two-pad model. [Gamepad API](https://wiki.libsdl.org/SDL3/CategoryGamepad), [rumble](https://wiki.libsdl.org/SDL3/SDL_RumbleGamepad). |
| Audio | Native PS1 sound decode and mixing to PCM; SDL3 audio stream for playback | SDL3 plays PCM but does not implement the game's SPU/VAB/SEQ or CD-XA behavior. Port those behaviors and feed the resulting samples to the [SDL3 audio stream](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream). |
| Disc data | Player-supplied disc; reuse the existing Python extractors on Windows during development | `tools/psx_disc.py` and `tools/afile.py` already describe the raw disc and archive. The game executable must not require Python at runtime or ship original assets. |
| Save/load | Virtual PS1 memory-card slots with the existing save-file layout; write under SDL3's per-user data directory | Preserve save bytes and in-game save behavior while using a writable Windows location. Write a temporary file, validate it, then replace the target and retain a backup. [SDL3 preference path](https://wiki.libsdl.org/SDL3/SDL_GetPrefPath). |

Use one native executable with clear platform adapters. The PS1 overlays become
host-side scene modules; do not execute MIPS overlays or assume their fixed
addresses are valid Windows pointers. Represent PS1 addresses and serialized
fields with explicit 32-bit types, and check structure sizes at compile time.

## Milestones

Milestone 0 now has a native build and empty window. Milestone 1 maps the PS1
boundaries and has a development-time Windows disc importer. The developer's
window and own-disc checks are pending; gameplay systems still need porting.

| Order | Work | Agent's code or synthetic exit check | Developer's actual check |
| --- | --- | --- | --- |
| **0. Native Windows tools first** | Use the installed Visual Studio C++ tools. Add a separate native CMake project and Debug/Release presets. Provide a small PowerShell build/test entry point that finds VS tools without requiring PATH edits. Pin SDL3 and build a minimal native `.exe`. Keep Python only for development/import tools. If the PS1 reference still needs Docker, offer an optional PowerShell wrapper around its existing check. | From ordinary PowerShell, configure and compile x64 Debug and Release with MSVC, then run headless synthetic tests. The native build and executable require no WSL, MSYS, or Docker. | Confirm the minimal window opens and closes on Windows. |
| **1. Data and PS1 boundaries** | Map the PSY-Q calls actually used, fixed-address memory, overlay lifetimes, disc sectors, and `A_FILE`. Adapt the existing Python import path for native Windows; plan a standalone importer before distributing the port. | Synthetic disc/archive fixtures round-trip; invalid input fails cleanly; address and layout assertions pass. | Confirm the supplied disc is recognized. |
| **2. Original graphics** | Reproduce the required GTE math and GPU primitives, VRAM/CLUT rules, ordering, transparency, and scene-specific display modes. Present completed frames with SDL3 at original internal resolution. | Golden pixel/frame tests cover primitives and palettes. Measure software-renderer time; change backend only if the original-mode frame budget cannot be met. | Compare title, menus, and scenes visually with the original. |
| **3. Input and clock** | Map gamepad, keyboard, and rumble to `pad.c` semantics. Use a fixed simulation tick derived from the game's VSync behavior, independent of host refresh rate. | Inject synthetic button sequences and clock ticks; verify repeats, scene transitions, and pause behavior. | Check controller feel and pacing. |
| **4. Audio** | Decode/mix the game's SPU samples, VAB/SEQ playback, reverb behaviors used here, and XA streams; send PCM to SDL3. | Decode fixtures and sample hashes, channel/fade tests, and underrun checks. | Compare music, effects, speech/streams, and timing by ear. |
| **5. Save/load** | Keep two virtual card slots and the original file format/UI semantics. Support import/export of the player's own save files. | Serialize/deserialize round-trips, corruption cases, and interrupted-write recovery. | Save, reload, and compare farms in game. |
| **6. Scene coverage and release** | Bring up farm, events, minigames, ending, and card flows. Keep Debug diagnostics out of Release; optimize only after measuring. | Scene-state and asset checks pass; both profiles compile; Release contains no developer diagnostics. | Perform full gameplay and audiovisual acceptance. |

The graphics/GTE and audio replacements are the main technical risks. Do the
smallest vertical slice of each early; do not equate a compiled `.exe` with a
playable port. Agents provide code-level evidence, while the developer owns
actual Windows and gameplay testing.
