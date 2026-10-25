# Configure, build and test the Windows foundation without a system installation.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'setup-toolchain.ps1')
$portRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $portRoot
try {
    & cmake --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
    & cmake --build --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    & ctest --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed' }
} finally {
    Pop-Location
}
