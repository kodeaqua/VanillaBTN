param(
    [Parameter(Mandatory = $true)]
    [string]$Disc,
    [string]$Out = 'build/game-data'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$discPath = (Resolve-Path -LiteralPath $Disc).Path
$outPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)
$discDir = Join-Path $outPath 'disc'
$memberDir = Join-Path $outPath 'members'

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw 'Python 3 is required for the development-time disc import.'
}

& python (Join-Path $PSScriptRoot 'psx_disc.py') extract $discPath --out $discDir
if ($LASTEXITCODE -ne 0) { throw 'Disc extraction failed.' }
foreach ($name in @('SLUS_011.15', 'A_FILE.BIN', 'A_FILE.HDT')) {
    if (-not (Test-Path -LiteralPath (Join-Path $discDir $name))) {
        throw "The disc is missing $name. Use the USA SLUS_011.15 disc image."
    }
}

& python (Join-Path $PSScriptRoot 'afile.py') unpack `
    --members (Join-Path $root 'disc/a_file.txt') `
    --bin (Join-Path $discDir 'A_FILE.BIN') `
    --hdt (Join-Path $discDir 'A_FILE.HDT') `
    --out $memberDir
if ($LASTEXITCODE -ne 0) { throw 'A_FILE unpack failed.' }
