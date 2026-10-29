# Configure, build and test the Windows port without a system installation.
param(
    [ValidateSet('windows-release', 'windows-asan-ubsan')]
    [string]$Preset = 'windows-release',
    [switch]$Game
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'setup-toolchain.ps1')
$portRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$repoRoot = (Resolve-Path (Join-Path $portRoot '..')).Path
Push-Location $portRoot
try {
    # A compiler upgrade can make CMake discard cached preset options mid-configure.
    # Start from the preset so sanitizer/release options cannot silently disappear.
    $configureArgs = @('--fresh', '--preset', $Preset)
    if ($Game) {
        $configureArgs += '-DHMBTN_BUILD_NATIVE_WINDOW=ON'
        $configureArgs += '-DHMBTN_ENH_PAUSE_MENU=ON'
        $configureArgs += '-DHMBTN_ENH_INTERPOLATION=ON'
        $configureArgs += '-DHMBTN_ENH_SCALING=ON'
        $configureArgs += '-DHMBTN_ENH_FILTERING=ON'
        $configureArgs += '-DHMBTN_ENH_WIDESCREEN=ON'
        $configureArgs += '-DHMBTN_ENH_ANTIALIASING=ON'
        $configureArgs += '-DHMBTN_ENH_GAMEPLAY=ON'
        $configureArgs += '-DHMBTN_ENH_PGXP=ON'
        $configureArgs += '-DHMBTN_ENH_DITHERING=ON'
    }
    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
    & cmake --build --preset $Preset -j12
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }

    if ($Game) {
        $gameExe = Join-Path $repoRoot "build/port/$Preset/bin/VanillaBTN.exe"
        if (-not (Test-Path $gameExe)) {
            $gameExe = Join-Path $repoRoot "build/port/$Preset/bin/hmbtn_native.exe"
        }
        if (Test-Path $gameExe) {
            $distDir = Join-Path $repoRoot "dist/VanillaBTN-windows-x64"
            if (Test-Path $distDir) {
                Remove-Item -Recurse -Force $distDir
            }
            New-Item -ItemType Directory -Force -Path $distDir | Out-Null
            Copy-Item -Path $gameExe -Destination (Join-Path $distDir "VanillaBTN.exe") -Force

            $readmeContent = @"
VanillaBTN - Harvest Moon: Back to Nature Native PC Port (v0.9.0)
==================================================================

How to Play:
1. Double-click 'VanillaBTN.exe' to launch the game.
2. On first launch, select your legally owned original PS1 Back to Nature disc image (.bin/.cue or .iso).
3. The game launches directly in clean single-window mode (Release mode).

Optional Quality of Life Features (Configurable via ESC Pause Menu):
- 16:9 Widescreen display and smooth fullscreen viewport scaling.
- Dedicated in-game pause menu (Graphics, Controls, independent BGM/SFX Audio, Gameplay).
- Tasteful QoL: 10-heart Stamina HUD, Tool Charge Meter, Tile Targeting Grid, Fast Text, etc.

Default Controls:
- Gamepad: Left Stick / D-Pad (Move), Button A (Action), Button X (Tool).
- Keyboard:
  * Preset A: Arrow Keys (Move), X Key (Action), Z Key (Tool).
  * Preset B: W/A/S/D (Move), K Key (Action), J Key (Tool).
- ESC: Open In-Game Pause Menu & Settings.

Saves & Settings Storage:
- Saves and settings.json are stored cleanly in:
  Documents\VanillaBTN\

Crash Diagnostics:
- In the unlikely event of an unexpected crash, a native dialog box will display the fault code and address for easy reporting.
"@
            Set-Content -Path (Join-Path $distDir "README.txt") -Value $readmeContent -Encoding UTF8

            $zipDir = Join-Path $repoRoot "dist"
            if (-not (Test-Path $zipDir)) {
                New-Item -ItemType Directory -Force -Path $zipDir | Out-Null
            }
            $zipPath = Join-Path $zipDir "VanillaBTN-windows-x64-v0.9.0.zip"
            if (Test-Path $zipPath) {
                Remove-Item -Force $zipPath
            }
            Compress-Archive -Path "$distDir\*" -DestinationPath $zipPath -Force
            Write-Host "`n[RELEASE PACKAGE SUCCESS]" -ForegroundColor Green
            Write-Host "Game Executable: $gameExe" -ForegroundColor Cyan
            Write-Host "Standalone ZIP:  $zipPath" -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
}
