# VanillaBTN

> **The Definitive, Authentic Native PC Port of Harvest Moon: Back to Nature**  
> *Preserving the true essence and childhood nostalgia of Mineral Town on modern hardware.*

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#building-from-source-windows)
[![Platform](https://img.shields.io/badge/platform-Windows%20x86--64-blue)](#quick-start-guide)
[![Tests](https://img.shields.io/badge/tests-121%2F121%20passing%20(100%25)-success)](#building-from-source-windows)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 🌾 The VanillaBTN Manifesto

Many modern decompilation and source-port projects for *Harvest Moon: Back to Nature* have unfortunately become bloated with out-of-place mods, aggressive cheats, and extraneous content that compromise the game's balance and ruin nostalgic authenticity.

**VanillaBTN was created to preserve that authentic purity.**

Our core design principles:
1. **100% Pure Vanilla Parity**: Simulation logic, NPC schedules, festivals, farming, animal husbandry, and narrative events execute byte-for-byte and tick-for-tick faithful to the original PlayStation 1 release (`SLUS_011.15`).
2. **Zero Bloat & No Game-Breaking Mods**: No intrusive foreign characters, immersion-breaking textures, or cheats that undermine the tranquil atmosphere of Mineral Town.
3. **Native 60 FPS Modern Engineering**: Not an emulator and not a slow static recompiler. The game is compiled natively from decompiled C source code coupled with a clean-room PS1 hardware runtime (GPU, SPU, GTE, Pad, Card, CD-ROM) that runs with buttery smoothness and minimal resource overhead on modern PCs.
4. **Tasteful & Optional Quality of Life (QoL)**: Every technical enhancement (widescreen, studio audio resampling, stamina HUD, tool charge indicator) is carefully styled to blend seamlessly with original PS1 aesthetics and **can be toggled or disabled at any time** via the in-game pause menu (ESC).

---

## ✨ Key Features

- **Clean Single-Window Native Launch**: The game launches directly into its dedicated window without annoying background command-prompt terminal popups.
- **Modern Display & Visual Fidelity**:
  - Native viewport resolution scaling, **16:9 Widescreen Mode**, and adaptive fullscreen presentation.
  - **True 32-bit Micro-Dither Debanding**: Eliminates color banding artifacts across night skies, morning fog, and dusk horizon gradients.
  - Optional bilinear texture filtering and FXAA / MSAA antialiasing.
  - Precise VSync and display refresh frame pacing without accelerating game simulation speed.
- **Studio-Quality Audio**:
  - High-fidelity audio resampling powered by a *Catmull-Rom Cubic Spline* algorithm delivering crisp treble and acoustic instrument clarity.
  - Independent volume sliders for Background Music (BGM) and Sound Effects (SFX).
- **Flexible Modern Input Support**:
  - Full native Gamepad / Controller support (Xbox, DualShock/DualSense, generic XInput/DirectInput).
  - Keyboard mappings: **Preset A** (Arrows + ZX) and **Preset B** (WASD + JK).
  - Optional Mouse mode for quick interactions.
- **Authentic In-Game Pause Interface (ESC Key)**:
  - In-game pause menu rendered with the original game's typography and dialogue styling to configure Graphics, Controls, Audio, and Gameplay on the fly.
- **Elegant QoL Enhancements (Optional - Default OFF)**:
  - **Stamina HUD**: 10-heart visual indicator in the top-left (automatically hidden during fullscreen menus and dialogue).
  - **Tool Charge Bar**: Floating charge meter above Pete's head with tier-specific ore color progression (Iron, Copper, Silver, Gold, Mystrile) and max-charge shimmer.
  - **Tile Targeting Grid**: Subtle 3D isometric highlight projecting the targeted soil tile in the farm field and greenhouse.
  - **Fast Text**: Instant dialogue text delivery with responsive button handling.
  - Walk through crops, durable fences, and smart animal bells.
- **Isolated User Data Storage**:
  - Saves and configuration are saved cleanly under `Documents\VanillaBTN\`.
  - Automatic, seamless migration preserves existing player data from previous versions.
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

## 🛠️ Building from Source (Windows)

VanillaBTN compiles natively using an automated toolchain setup without requiring WSL, Docker containers, or pre-installed PSY-Q SDKs.

### Prerequisites:
- Windows 10 or 11 (64-bit).
- PowerShell 5.1 or newer.
- Git.

### Build Command:
Execute the following command in PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
```

This automated script will:
1. Provision local LLVM-MinGW (Clang), Ninja, and CMake toolchains without system pollution.
2. Compile the cow application icon, Windows GUI subsystem, and all runtime modules.
3. Run the automated test suite (121 unit tests, 100% passing).
4. Output the standalone `VanillaBTN.exe` binary to `build/port/windows-release/bin/`.
5. Package the release archive and README to `dist/VanillaBTN-windows-x64-v0.9.0.zip`.

---

## ⚖️ Legal & Licensing

- The native port source code of VanillaBTN is licensed under the **[MIT License](LICENSE)**.
- Derived from the *Harvest Moon: Back to Nature* decompilation project ([CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/)) and `maspsx` ([MIT](tools/maspsx/LICENSE)).
- **Copyright Notice**: This repository **does not contain** proprietary copyrighted game assets (CD audio, textures, STR FMVs, ROM/ISO images) belonging to Victor Interactive Software / Marvelous Inc. Players must supply their own legally owned original game copy to run the game.
