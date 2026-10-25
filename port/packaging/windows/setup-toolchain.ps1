# Prepare pinned, project-local Windows build tools in the current session.
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$toolRoot = Join-Path $repoRoot 'build/host-tools'
New-Item -ItemType Directory -Force $toolRoot | Out-Null
$packages = @(
    @{
        Archive = 'llvm-mingw.zip'
        Url = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20241001/llvm-mingw-20241001-ucrt-x86_64.zip'
        Sha256 = '09b9aecf4a9ceb4ad942aa8b6b6126895678383532f565e26219adc493e50466'
        Probe = 'llvm-mingw-20241001-ucrt-x86_64/bin/clang.exe'
    },
    @{
        Archive = 'cmake.zip'
        Url = 'https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-windows-x86_64.zip'
        Sha256 = 'd163cd3ab4959b0a53fa8988f2ddbd2e6c501658201e6a154386bad9dbe4f836'
        Probe = 'cmake-3.31.6-windows-x86_64/bin/cmake.exe'
    },
    @{
        Archive = 'ninja.zip'
        Url = 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip'
        Sha256 = 'f550fec705b6d6ff58f2db3c374c2277a37691678d6aba463adcbb129108467a'
        Probe = 'ninja.exe'
    }
)
foreach ($package in $packages) {
    $archive = Join-Path $toolRoot $package.Archive
    if (-not (Test-Path -LiteralPath $archive)) {
        $partial = "$archive.part"
        Invoke-WebRequest -UseBasicParsing -Uri $package.Url -OutFile $partial
        if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $package.Sha256) {
            throw "Checksum mismatch: $($package.Archive)"
        }
        Move-Item -LiteralPath $partial -Destination $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $package.Sha256) {
        throw "Checksum mismatch: $($package.Archive)"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $toolRoot $package.Probe))) {
        & tar.exe -xf $archive -C $toolRoot
        if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $($package.Archive)" }
    }
}
$toolPaths = @(
    (Join-Path $toolRoot 'llvm-mingw-20241001-ucrt-x86_64/bin'),
    (Join-Path $toolRoot 'cmake-3.31.6-windows-x86_64/bin'),
    $toolRoot
)
$env:PATH = ($toolPaths -join ';') + ';' + $env:PATH
Write-Host 'Windows tools ready in this PowerShell session. Run presets from port/.'
