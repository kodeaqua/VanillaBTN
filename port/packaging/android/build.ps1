# Configure, build and package VanillaBTN for Android.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'

# Set environment paths
$androidSdk = 'C:\Users\Aqua\AppData\Local\Android\Sdk'
$androidNdk = 'C:\Users\Aqua\AppData\Local\Android\Sdk\ndk\android-ndk-r27c'
$javaHome = 'C:\Users\Aqua\AppData\Local\Programs\Android Studio\jbr'

$env:JAVA_HOME = $javaHome
$env:ANDROID_HOME = $androidSdk
$env:ANDROID_SDK_ROOT = $androidSdk
$env:ANDROID_NDK_HOME = $androidNdk

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
    $gradleBat = 'C:\Users\Aqua\.gradle\wrapper\dists\gradle-9.6.0-bin\42k10rwplmzkhuboz9kdazi7s\gradle-9.6.0\bin\gradle.bat'
    if (-not (Test-Path $gradleBat)) {
        $gradleBat = 'C:\Users\Aqua\.gradle\wrapper\dists\gradle-8.13-bin\codex-install\gradle-8.13\bin\gradle.bat'
    }

    $taskName = "assemble$BuildType"
    & $gradleBat $taskName
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
