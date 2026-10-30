# VanillaBTN

> **The Definitive, Authentic Native PC Port of Harvest Moon: Back to Nature**  
> *Preserving the true essence and childhood nostalgia of Mineral Town on modern hardware.*

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#building-from-source-windows)
[![Platform](https://img.shields.io/badge/platform-Windows%20x86--64%20%7C%20Android%20arm64--v8a-blue)](#quick-start-guide)
[![Tests](https://img.shields.io/badge/tests-124%2F124%20passing%20(100%25)-success)](#building-from-source)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 🌾 The VanillaBTN Manifesto

Many modern decompilation and source-port projects for *Harvest Moon: Back to Nature* have unfortunately become bloated with out-of-place mods, aggressive cheats, and extraneous content that compromise the game's balance and ruin nostalgic authenticity.

**VanillaBTN was created to preserve that authentic purity.**

Our core design principles:
1. **Vanilla-first Defaults**: With optional enhancements disabled, the port preserves original game logic and timing where native compatibility is implemented. Full-playthrough parity remains unverified.
2. **Zero Bloat & No Game-Breaking Mods**: No intrusive foreign characters, immersion-breaking textures, or cheats that undermine the tranquil atmosphere of Mineral Town.
3. **Native Game Timing**: Not an emulator or static recompiler. The game is compiled from decompiled C source and uses a clean-room PS1 runtime (GPU, SPU, GTE, pad, memory card, and CD-ROM). Hardware timing remains fixed; presentation caps and optional visual interpolation do not accelerate simulation.
4. **Tasteful & Optional Enhancements**: Optional graphics and gameplay features can be configured in the pause menu. Gameplay enhancements retain their own settings; the requested ESC pause interface is available by default.

---

## ✨ Key Features

- **Clean Single-Window Native Launch**: The game launches directly into its dedicated window without annoying background command-prompt terminal popups.
- **Modern Display & Visual Fidelity**:
  - Optional viewport-resolution scaling, **16:9 Widescreen Mode**, and adaptive fullscreen presentation. Unsupported GPU work retains the Software reference path.
  - Optional dithering bypass and smoother shading on selected enhanced GPU paths. Other paths retain PS1 BGR555 color precision.
  - Optional bilinear texture filtering, FXAA, and GPU MSAA 2x/4x.
  - Precise VSync and display refresh frame pacing without accelerating game simulation speed.
- **Audio Options**:
  - PS1 Gaussian resampling is the default. Studio HQ Catmull-Rom resampling is an explicit opt-in.
  - Independent volume sliders for Background Music (BGM) and Sound Effects (SFX).
- **Flexible Modern Input Support**:
  - Full native Gamepad / Controller support (Xbox, DualShock/DualSense, generic XInput/DirectInput).
  - Keyboard mappings: **Preset A** (Arrows + ZX) and **Preset B** (WASD + JK).
  - Mouse navigation remains available in the pause menu. In-game mouse/touch clicks and mouse-to-gamepad input are temporarily disabled.
- **Authentic In-Game Pause Interface (ESC Key)**:
  - In-game pause menu rendered with the original game's typography and dialogue styling to configure Graphics, Controls, Audio, and Gameplay on the fly.
- **Optional Gameplay Enhancements (Default OFF)**:
  - **Stamina HUD**: 10-heart indicator with a fatigue gauge; visibility follows the configured HUD conditions.
  - **Tool Charge Bar**: Floating charge meter above Pete's head with tier-specific ore color progression (Iron, Copper, Silver, Gold, Mystrile) and max-charge shimmer.
  - **Tile Targeting Grid**: Isometric highlight for the targeted tile during eligible gameplay on the farm and in mines.
  - Walk through crops, durable fences, and smart animal bells.
- **Graphics Modes**: Native PS1, Enhance PS1, and Modern. Android offers the same selector with mobile-specific rendering limits.
- Fast Text is currently removed from the settings menu while multi-line dialogue pagination is reconsidered.
- **Isolated User Data Storage**:
  - Windows saves and configuration use the active user's `Documents\VanillaBTN\`; Android uses app-private storage.
  - Windows migrates the previous `Documents\BackToNature\` folder when needed.
- **Robust Crash Diagnostics**:
  - Displays a platform-native error dialog detailing the fault reason, exception code, and memory address if an unexpected crash occurs.

---

## 🚀 Quick Start Guide

1. Download the latest release package: **`VanillaBTN-windows-x64-v0.9.0.zip`** from [Releases](https://github.com/kodeaqua/VanillaBTN/releases).
2. Extract the ZIP archive to your preferred directory.
3. Provide your own original *Harvest Moon: Back to Nature (USA)* disc image (`.bin`/`.cue` or `.iso`).
4. Double-click **`VanillaBTN.exe`**:
   - On first launch, if prompted, select your `.bin` / `.cue` disc file.
   - The path is remembered automatically for all subsequent playthroughs.
5. Enjoy your life in Mineral Town with authentic nostalgic perfection!

---

## 🎮 Controls

| Action | Gamepad | Keyboard (Preset A) | Keyboard (Preset B - WASD) |
|---|---|---|---|
| **Move** | D-Pad / Left Stick | Arrow Keys | W / A / S / D |
| **Action / Talk / Pick Up** | Button A / Cross | X Key | K Key |
| **Use Tool** | Button X / Square | Z Key | J Key |
| **Open Bag / Rucksack** | Button Y / Triangle | W Key | I Key |
| **Eat / Put Away** | Button B / Circle | D Key | L Key |
| **Swap Item / Tool** | Button L1 / R1 | S Key | U Key |
| **Whistle (Horse / Dog)** | Button L2 / R2 | A Key | O Key |
| **Run** | Hold RB / R1 | Hold Shift / Auto | Hold Shift / Auto |
| **Pause Menu & Settings** | START Button | ESC Key | ESC Key |

---

## 🛠️ Building from Source

VanillaBTN compiles natively using automated toolchain scripts without requiring WSL, Docker containers, or pre-installed PSY-Q SDKs. See **[BUILDING.md](BUILDING.md)** for comprehensive guides, build options, and prerequisites.

### Windows (x86-64 Native)
Run the following command in PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
```

This automated script will:
1. Provision local LLVM-MinGW (Clang), Ninja, and CMake toolchains without system pollution.
2. Compile the application icon, Windows GUI subsystem, and all runtime modules.
3. Run the automated test suite (124 unit tests, 100% passing).
4. Output the standalone `VanillaBTN.exe` binary to `build/port/windows-release/bin/`.
5. Package the release archive and README to `dist/VanillaBTN-windows-x64-v0.9.0.zip`.

### Android (arm64-v8a Native)
Run the following command in PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/android/build.ps1 -BuildType Release
```

Output APK will be generated at `dist/VanillaBTN-android-v0.9.0.apk`.

---

## ⚖️ Legal & Licensing

- The native port source code of VanillaBTN is licensed under the **[MIT License](LICENSE)**.
- Derived from the *Harvest Moon: Back to Nature* decompilation project ([CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)) and `maspsx` ([MIT](tools/maspsx/LICENSE)).
- **Copyright Notice**: This repository **does not contain** proprietary copyrighted game assets (CD audio, textures, STR FMVs, ROM/ISO images) belonging to Victor Interactive Software / Marvelous Inc. Players must supply their own legally owned original game copy to run the game.
