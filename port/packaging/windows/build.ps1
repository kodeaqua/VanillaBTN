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
VanillaBTN - Harvest Moon: Back to Nature Native Port (v0.9.0)
==============================================================

Petunjuk Menjalankan Game:
1. Klik ganda 'VanillaBTN.exe' untuk memulai.
2. Pada peluncuran pertama, pilih file disc image original PS1 Back to Nature (.bin/.cue atau .iso).
3. Game langsung berjalan mulus dalam satu window mandiri (Release mode).

Fitur & Peningkatan Opsional (dapat diatur lewat menu ESC):
- Tampilan Widescreen 16:9 & Fullscreen scaling halus.
- In-game Pause Menu (Grafis, Kontrol, Audio BGM/SFX terpisah, Gameplay).
- Fitur QoL: 10-Hati Stamina HUD, Tool Charge Gauge, Tile Targeting Grid, Fast Text, dsb.

Kontrol Default:
- Gamepad: Left Stick / D-Pad (Bergerak), Tombol A (Aksi), Tombol X (Tool).
- Keyboard:
  * Preset A: Tombol Panah (Bergerak), X (Aksi), Z (Tool).
  * Preset B: W/A/S/D (Bergerak), K (Aksi), J (Tool).
- ESC: Membuka In-Game Pause Menu & Pengaturan.

Penyimpanan Save & Konfigurasi:
- Save game dan settings.json tersimpan otomatis di:
  Documents\VanillaBTN\

Crash Reporting:
- Jika terjadi crash tidak terduga, dialog pesan error akan muncul menampilkan kode dan alamat fault.
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
