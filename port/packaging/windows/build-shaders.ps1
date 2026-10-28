# Rebuild this repository's own shaders. Normal game builds use checked-in C
# byte arrays and do not download, link or distribute the shader compiler.
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$toolRoot = Join-Path $repoRoot 'build/host-tools'
$archive = Join-Path $toolRoot 'dxc_2025_07_14.zip'
$expected = '9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4'
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip' -OutFile "$archive.part"
    if ((Get-FileHash -LiteralPath "$archive.part" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'DXC checksum mismatch' }
    Move-Item -LiteralPath "$archive.part" -Destination $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'DXC checksum mismatch' }
$compilerRoot = Join-Path $toolRoot 'dxc-1.8.2505.1'
$compiler = Join-Path $compilerRoot 'bin/x64/dxc.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    New-Item -ItemType Directory -Force -Path $compilerRoot | Out-Null
    & tar.exe -xf $archive -C $compilerRoot
    if ($LASTEXITCODE -ne 0) { throw 'DXC extraction failed' }
}
$outputRoot = Join-Path $repoRoot 'build/port/shaders'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)
Push-Location $repoRoot
try {
    foreach ($program in @('scene', 'fxaa')) {
    $folder = if ($program -eq 'scene') { 'port/gfx/shaders' } else { 'port/enhance/antialiasing' }
    $shaderPath = "$folder/$program.hlsl"
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot $shaderPath)).Replace("`r`n", "`n")
    if ($program -eq 'scene') {
        $source += "`n/* filtering */`n" + [IO.File]::ReadAllText((Join-Path $repoRoot 'port/enhance/filtering/texture.hlsl')).Replace("`r`n", "`n")
        $source += "`n/* multisampling */`n" + [IO.File]::ReadAllText((Join-Path $repoRoot 'port/enhance/antialiasing/msaa.hlsl')).Replace("`r`n", "`n")
    }
    $hasher = [Security.Cryptography.SHA256]::Create()
    $sourceHash = -join ($hasher.ComputeHash($utf8.GetBytes($source)) | ForEach-Object { $_.ToString('x2') })
    $hasher.Dispose()
    foreach ($format in @('spirv', 'dxil')) {
        $binary = Join-Path $outputRoot "$program.$format"
        $arguments = @('-T', 'cs_6_0', '-E', 'main', '-O3', '-Qstrip_debug', '-Fo', $binary)
        if ($format -eq 'spirv') { $arguments += @('-spirv', '-fspv-target-env=vulkan1.0') }
        & $compiler @arguments $shaderPath
        if ($LASTEXITCODE -ne 0) { throw "Shader compilation failed: $program $format" }
        $bytes = [IO.File]::ReadAllBytes($binary)
        $text = New-Object Text.StringBuilder
        [void]$text.Append("/* Normalized source SHA256: $sourceHash */`n/* Own HLSL; DXC v1.8.2505.1. */`n")
        [void]$text.Append("_Alignas(4) static const unsigned char hm_${program}_$format[] = {`n")
        for ($i = 0; $i -lt $bytes.Length; $i++) {
            if ($i % 16 -eq 0) { [void]$text.Append('    ') }
            if ($i % 16 -ne 0) { [void]$text.Append(' ') }
            [void]$text.Append(('0x{0:x2},' -f $bytes[$i]))
            if ($i % 16 -eq 15) { [void]$text.Append("`n") }
        }
        [void]$text.Append("`n};`n")
        [IO.File]::WriteAllText((Join-Path $repoRoot "$folder/${program}_$format.h"), $text.ToString(), $utf8)
    }
    }
} finally { Pop-Location }
Write-Host 'Own SPIR-V/DXIL shader headers regenerated.'
