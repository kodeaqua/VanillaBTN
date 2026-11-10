param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]*$')]
    [string]$ExecutableName = 'vanillabtn'
)

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer (vswhere.exe) was not found.'
}
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw 'Visual Studio with Desktop development with C++ was not found.'
}
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
foreach ($tool in @($vcvars, $cmake, $ctest, $ninja)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Required Visual Studio build tool was not found: $tool"
    }
}

# Import the x64 compiler environment into this PowerShell process only.
$vcEnvironment = & $env:ComSpec /d /s /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw 'Could not initialize the Visual Studio x64 compiler environment.'
}
foreach ($entry in $vcEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
$env:PATH = "$(Split-Path $ninja);$env:PATH"

$nativeDir = Join-Path (Split-Path $PSScriptRoot) 'native'
Push-Location $nativeDir
try {
    & $cmake --preset $Configuration "-DVANILLABTN_EXECUTABLE_NAME=$ExecutableName"
    if ($LASTEXITCODE -ne 0) { throw 'Native CMake configure failed.' }
    & $cmake --build "../build/native/$Configuration"
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    & $ctest --test-dir "../build/native/$Configuration" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Native synthetic test failed.' }
} finally {
    Pop-Location
}
