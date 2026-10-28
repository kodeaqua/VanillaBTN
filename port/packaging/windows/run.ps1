# Launch the native Windows game with the owner's local, validated USA BIN.
# Without -DiscPath the game uses the path remembered in settings.json or a .bin next to the exe.
param(
    [string]$DiscPath
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$discArguments = @()
if ($DiscPath) {
    $disc = (Resolve-Path -LiteralPath $DiscPath).Path
    if (-not (Test-Path -LiteralPath $disc -PathType Leaf)) { throw 'DiscPath must name a BIN file' }
    $discArguments = @($disc)
}
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
    & $game @discArguments
    if ($LASTEXITCODE -ne 0) { throw "Native game exited with code $LASTEXITCODE" }
} finally {
    foreach ($name in $diagnosticNames) {
        [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process')
    }
}
