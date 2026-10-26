# Configure, build and test the Windows port without a system installation.
param(
    [ValidateSet('windows-release', 'windows-asan-ubsan')]
    [string]$Preset = 'windows-release',
    [switch]$Game
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'setup-toolchain.ps1')
$portRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $portRoot
try {
    # A compiler upgrade can make CMake discard cached preset options mid-configure.
    # Start from the preset so sanitizer/release options cannot silently disappear.
    $configureArgs = @('--fresh', '--preset', $Preset)
    if ($Game) {
        $configureArgs += '-DHMBTN_BUILD_NATIVE_WINDOW=ON'
        $configureArgs += '-DHMBTN_ENH_PAUSE_MENU=ON'
    }
    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }
} finally {
    Pop-Location
}
