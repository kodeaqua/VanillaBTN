# Launch the native Windows game with the owner's local, validated USA BIN.
param(
    [Parameter(Mandatory = $true)]
    [string]$DiscPath
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$disc = (Resolve-Path -LiteralPath $DiscPath).Path
if (-not (Test-Path -LiteralPath $disc -PathType Leaf)) { throw 'DiscPath must name a BIN file' }
$game = Join-Path $repoRoot 'build/port/windows-release/hmbtn_native.exe'
if (-not (Test-Path -LiteralPath $game)) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset windows-release -Game
}
# Diagnostic replays and tick limits must not replace the player's keyboard.
$diagnosticNames = @('HMBTN_INPUT_REPLAY', 'HMBTN_BOOT_TICKS', 'HMBTN_CAPTURE_FRAME',
    'HMBTN_TRACE_SCANOUT', 'HMBTN_TRACE_GAME_STATE')
$previous = @{}
try {
    foreach ($name in $diagnosticNames) {
        $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    & $game $disc
    if ($LASTEXITCODE -ne 0) { throw "Native game exited with code $LASTEXITCODE" }
} finally {
    foreach ($name in $diagnosticNames) {
        [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process')
    }
}
