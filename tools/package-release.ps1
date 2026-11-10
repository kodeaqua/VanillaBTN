param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$OutDir = 'dist',
    [switch]$SkipBuild,
    [switch]$NoZip
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot

if (-not $SkipBuild) {
    Write-Host "Building VanillaBTN ($Configuration)..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'build-windows.ps1') -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

    Write-Host "Running tool test suites..." -ForegroundColor Cyan
    & python -m unittest discover (Join-Path $PSScriptRoot 'tests')
    if ($LASTEXITCODE -ne 0) { throw 'Tool tests failed.' }
}

$version = (& python (Join-Path $PSScriptRoot 'version.py')).Trim()
if (-not $version) { throw 'Could not determine version.' }

$packageName = "VanillaBTN-v$version-win64"
$distRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
$targetDir = Join-Path $distRoot $packageName

Write-Host "Assembling release package in $targetDir..." -ForegroundColor Cyan
if (Test-Path -LiteralPath $targetDir) {
    Remove-Item -LiteralPath $targetDir -Recurse -Force
}
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

# 1. Copy executable
$exeSource = Join-Path $repoRoot "build/native/$Configuration/vanillabtn.exe"
if (-not (Test-Path -LiteralPath $exeSource)) {
    throw "Executable not found at $exeSource"
}
Copy-Item -LiteralPath $exeSource -Destination (Join-Path $targetDir 'vanillabtn.exe')

# 2. Copy License
$licenseSource = Join-Path $repoRoot 'LICENSE'
if (Test-Path -LiteralPath $licenseSource) {
    Copy-Item -LiteralPath $licenseSource -Destination (Join-Path $targetDir 'LICENSE.txt')
}

# 3. Create README.txt with quick-start and controls guide
$readmeContent = @"
======================================================================
  VanillaBTN - Native Windows Bringup of Harvest Moon: Back to Nature
  Version: $version ($Configuration)
======================================================================

OVERVIEW
--------
VanillaBTN is a native Windows x64 executable running the original
game code of Harvest Moon: Back to Nature (USA SLUS_011.15) with native
platform services (SDL3 audio, video, input, and memory card saves).

QUICK START
-----------
1. VanillaBTN requires original game assets from your legally owned
   Harvest Moon: Back to Nature (USA) PS1 disc.

2. Import your disc using the included helper script:
     powershell -ExecutionPolicy Bypass -File tools/import-game.ps1 -Disc "path\to\game.bin" -Out "data"
   
   Or manually place A_FILE.BIN and A_FILE.HDT into a folder named
   "disc" or "data" next to vanillabtn.exe.

3. Launch vanillabtn.exe:
     .\vanillabtn.exe
   
   To specify a custom game data folder or memory card folder:
     .\vanillabtn.exe --data "path\to\disc_folder" --card-dir "path\to\saves"

DEFAULT CONTROLS
----------------
Gamepad (DualShock / Xbox / XInput / DirectInput):
  - D-Pad / Left Stick : Move character / Navigate menus
  - Cross (A)          : Talk / Check / Use equipped tool (Action)
  - Square (X)         : Cancel / Put item into rucksack
  - Triangle (Y)       : Open rucksack / inventory menu
  - Circle (B)         : Eat / Pick up / Drop item
  - L1 (LB)            : Whistle (Call dog / horse)
  - R1 (RB)            : Run
  - L2 (LT)            : Cycle tool
  - R2 (RT)            : Cycle item
  - Start              : Open main farm status menu
  - Select             : Open town map

Keyboard:
  - Arrow Keys / WASD  : Move / Navigate menus
  - X / K / Enter      : Action / Confirm / Use tool (PS1 Cross)
  - C / U              : Cancel / Put item into rucksack (PS1 Square)
  - V / I              : Open rucksack / inventory menu (PS1 Triangle)
  - Z / J              : Eat / Pick up / Drop item (PS1 Circle)
  - Q                  : Whistle (PS1 L1)
  - E                  : Run (PS1 R1)
  - 1                  : Cycle tool (PS1 L2)
  - 2                  : Cycle item (PS1 R2)
  - Space              : Open main farm status menu (PS1 Start)
  - Escape / Tab       : Open town map (PS1 Select)

SAVES
-----
Saves are stored in standard 128 KB PS1 virtual memory card images
in your user profile's local preference directory or the folder
specified by --card-dir. Dual virtual card slots are supported.

LICENSE & CREDITS
-----------------
See LICENSE.txt for license terms.
This software contains no copyrighted game assets or Sony SDK files.
"@
Set-Content -LiteralPath (Join-Path $targetDir 'README.txt') -Value $readmeContent -Encoding utf8

# 4. Copy disc import tooling
$toolsDir = Join-Path $targetDir 'tools'
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
foreach ($script in @('import-game.ps1', 'psx_disc.py', 'afile.py')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination (Join-Path $toolsDir $script)
}
$discDir = Join-Path $targetDir 'disc'
New-Item -ItemType Directory -Path $discDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'disc/a_file.txt') -Destination (Join-Path $discDir 'a_file.txt')

# 5. Create ZIP archive
if (-not $NoZip) {
    $zipPath = Join-Path $distRoot "$packageName.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Write-Host "Compressing archive to $zipPath..." -ForegroundColor Cyan
    Compress-Archive -Path "$targetDir\*" -DestinationPath $zipPath -Force
    $zipSize = (Get-Item -LiteralPath $zipPath).Length / 1MB
    Write-Host ("Created {0} ({1:N2} MB)" -f $zipPath, $zipSize) -ForegroundColor Green
}

Write-Host "Package assembled successfully: $targetDir" -ForegroundColor Green
