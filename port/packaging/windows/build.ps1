# Configure, build and test the Windows port without a system installation.
param(
    [ValidateSet('windows-release', 'windows-asan-ubsan')]
    [string]$Preset = 'windows-release'
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'setup-toolchain.ps1')
$portRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $portRoot
try {
    # A compiler upgrade can make CMake discard cached preset options mid-configure.
    # Start from the preset so sanitizer/release options cannot silently disappear.
    & cmake --fresh --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }
} finally {
    Pop-Location
}
