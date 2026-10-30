# Configure, build and package VanillaBTN for Android.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'

# Set environment paths dynamically with fallbacks
$androidSdk = if ($env:ANDROID_HOME -and (Test-Path $env:ANDROID_HOME)) { $env:ANDROID_HOME }
              elseif ($env:ANDROID_SDK_ROOT -and (Test-Path $env:ANDROID_SDK_ROOT)) { $env:ANDROID_SDK_ROOT }
              elseif (Test-Path "$env:LOCALAPPDATA\Android\Sdk") { "$env:LOCALAPPDATA\Android\Sdk" }
              else { 'C:\Users\Aqua\AppData\Local\Android\Sdk' }

$androidNdk = if ($env:ANDROID_NDK_HOME -and (Test-Path $env:ANDROID_NDK_HOME)) { $env:ANDROID_NDK_HOME }
              elseif ($androidSdk -and (Test-Path (Join-Path $androidSdk 'ndk'))) {
                  $found = Get-ChildItem (Join-Path $androidSdk 'ndk') -Directory | Sort-Object Name -Descending
                  if ($found) { $found[0].FullName } else { $null }
              }
              else { 'C:\Users\Aqua\AppData\Local\Android\Sdk\ndk\android-ndk-r27c' }

$javaHome = if ($env:JAVA_HOME -and (Test-Path $env:JAVA_HOME)) { $env:JAVA_HOME }
            elseif (Test-Path 'C:\DevTools\jdk') { 'C:\DevTools\jdk' }
            elseif (Test-Path "$env:LOCALAPPDATA\Programs\Android Studio\jbr") { "$env:LOCALAPPDATA\Programs\Android Studio\jbr" }
            elseif (Test-Path "C:\Program Files\Android\Android Studio\jbr") { "C:\Program Files\Android\Android Studio\jbr" }
            else { 'C:\Users\Aqua\AppData\Local\Programs\Android Studio\jbr' }

$env:JAVA_HOME = $javaHome
$env:ANDROID_HOME = $androidSdk
$env:ANDROID_SDK_ROOT = $androidSdk
if ($androidNdk) { $env:ANDROID_NDK_HOME = $androidNdk }

$portAndroidDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $portAndroidDir '../../..')).Path
$toolRoot = Join-Path $repoRoot 'build/host-tools'
$cmakeBin = Join-Path $toolRoot 'cmake-3.31.6-windows-x86_64/bin'

$env:PATH = "$cmakeBin;$toolRoot;$env:PATH"

Write-Host "=========================================================="
Write-Host "VanillaBTN Android Build ($BuildType)"
Write-Host "Java:    $javaHome"
Write-Host "SDK:     $androidSdk"
Write-Host "NDK:     $androidNdk"
Write-Host "Package: io.github.kodeaqua.vanillabtn"
Write-Host "=========================================================="

Push-Location $portAndroidDir
try {
    $gradleBat = if (Get-Command gradle -ErrorAction SilentlyContinue) { (Get-Command gradle).Source }
                 elseif (Test-Path "$env:USERPROFILE\.gradle\wrapper\dists") {
                     $g = Get-ChildItem -Path "$env:USERPROFILE\.gradle\wrapper\dists" -Filter "gradle.bat" -Recurse -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
                     if ($g) { $g[0].FullName } else { $null }
                 }
                 else { $null }

    if (-not $gradleBat -or -not (Test-Path $gradleBat)) {
        $gradleBat = 'C:\Users\Aqua\.gradle\wrapper\dists\gradle-9.6.0-bin\42k10rwplmzkhuboz9kdazi7s\gradle-9.6.0\bin\gradle.bat'
        if (-not (Test-Path $gradleBat)) {
            $gradleBat = 'C:\Users\Aqua\.gradle\wrapper\dists\gradle-8.13-bin\codex-install\gradle-8.13\bin\gradle.bat'
        }
    }

    $taskName = "assemble$BuildType"
    $gradleArgs = @($taskName)
    if ($androidNdk) {
        $ndkVer = Split-Path $androidNdk -Leaf
        $gradleArgs += "-PndkVersion=$ndkVer"
    }

    & $gradleBat @gradleArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Android Gradle build failed with exit code $LASTEXITCODE"
    }

    $apkSubdir = $BuildType.ToLower()
    $apkPath = Join-Path $portAndroidDir "app/build/outputs/apk/$apkSubdir/app-$apkSubdir.apk"
    if (-not (Test-Path $apkPath)) {
        $apkPath = Join-Path $portAndroidDir "app/build/outputs/apk/$apkSubdir/app-$apkSubdir-unsigned.apk"
    }

    if (Test-Path $apkPath) {
        $distDir = Join-Path $repoRoot "dist"
        if (-not (Test-Path $distDir)) {
            New-Item -ItemType Directory -Force -Path $distDir | Out-Null
        }
        $finalApkName = "VanillaBTN-android-v0.9.0.apk"
        $finalApkPath = Join-Path $distDir $finalApkName
        Copy-Item -Path $apkPath -Destination $finalApkPath -Force

        $fileSizeMB = [math]::Round((Get-Item $finalApkPath).Length / 1MB, 2)
        Write-Host ""
        Write-Host "[ANDROID BUILD SUCCESS]"
        Write-Host "Output APK: $finalApkPath ($fileSizeMB MB)"
        Write-Host "=========================================================="
    } else {
        Write-Warning "Could not find output APK at expected path: $apkPath"
    }
} finally {
    Pop-Location
}
