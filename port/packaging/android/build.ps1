# Configure, build and package VanillaBTN for Android.
param(
    [ValidateSet('Debug', 'Release')]
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'
$requiredNdkVersion = '30.0.16248370'
$requiredGradleVersion = '9.8.2'

function Get-NdkRevision([string]$ndkPath) {
    $propertiesPath = Join-Path $ndkPath 'source.properties'
    if (-not (Test-Path $propertiesPath)) { return $null }
    $revisionLine = Select-String -Path $propertiesPath -Pattern '^\s*Pkg\.Revision\s*=\s*(\d+\.\d+\.\d+)\s*$' |
        Select-Object -First 1
    if ($revisionLine -and $revisionLine.Matches[0].Groups[1].Success) {
        return $revisionLine.Matches[0].Groups[1].Value
    }
    return $null
}

$androidSdkCandidates = @(
    @(
        $env:ANDROID_HOME,
        $env:ANDROID_SDK_ROOT,
        $(if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'Android/Sdk' })
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique
)
if (-not $androidSdkCandidates) {
    throw 'Android SDK not found. Set ANDROID_HOME or ANDROID_SDK_ROOT.'
}
$androidSdk = $androidSdkCandidates[0]

$androidNdk = Join-Path (Join-Path $androidSdk 'ndk') $requiredNdkVersion
$installedNdkVersion = Get-NdkRevision $androidNdk
if ($installedNdkVersion -ne $requiredNdkVersion) {
    throw "Android NDK $requiredNdkVersion is required. Install it with sdkmanager --install 'ndk;$requiredNdkVersion'."
}

# Let android.ndkVersion select the pinned SDK NDK; remove a stale legacy override.
$localProperties = Join-Path $PSScriptRoot 'local.properties'
if (Test-Path $localProperties) {
    $localPropertiesText = [IO.File]::ReadAllText($localProperties)
    $updatedLocalProperties = [regex]::Replace($localPropertiesText, '(?m)^ndk\.dir=.*(?:\r?\n|$)', '')
    if ($updatedLocalProperties -ne $localPropertiesText) {
        [IO.File]::WriteAllText($localProperties, $updatedLocalProperties, [Text.UTF8Encoding]::new($false))
    }
}

$javaCandidates = @(
    @(
        $env:JAVA_HOME,
        $(if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'Programs/Android Studio/jbr' }),
        'C:\Program Files\Android\Android Studio\jbr'
    ) | Where-Object { $_ -and (Test-Path (Join-Path $_ 'bin/java.exe')) } | Select-Object -Unique
)
if ($javaCandidates) {
    $env:JAVA_HOME = $javaCandidates[0]
} elseif (-not (Get-Command java -ErrorAction SilentlyContinue)) {
    throw 'Java was not found. Set JAVA_HOME to a supported JDK or add java.exe to PATH.'
}

$env:ANDROID_HOME = $androidSdk
$env:ANDROID_SDK_ROOT = $androidSdk
$env:ANDROID_NDK_HOME = $androidNdk

$portAndroidDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $portAndroidDir '../../..')).Path
$toolRoot = Join-Path $repoRoot 'build/host-tools'
$cmakeBin = Join-Path $toolRoot 'cmake-3.31.6-windows-x86_64/bin'
if (-not (Test-Path (Join-Path $cmakeBin 'cmake.exe'))) {
    $cmakeBin = Join-Path $androidSdk 'cmake/3.31.6/bin'
}
if (Test-Path (Join-Path $cmakeBin 'cmake.exe')) {
    $env:PATH = "$cmakeBin;$toolRoot;$env:PATH"
}

$gradleWrapper = Join-Path $portAndroidDir 'gradlew.bat'
$wrapperProperties = Join-Path $portAndroidDir 'gradle/wrapper/gradle-wrapper.properties'
if (-not (Test-Path $gradleWrapper) -or -not (Test-Path $wrapperProperties)) {
    throw 'Pinned Gradle wrapper is missing from port/packaging/android.'
}
if (-not (Select-String -Path $wrapperProperties -Pattern "gradle-$requiredGradleVersion-bin\.zip" -Quiet)) {
    throw "Gradle wrapper must use version $requiredGradleVersion."
}

Write-Host '=========================================================='
Write-Host "VanillaBTN Android Build ($BuildType)"
Write-Host "Java:    $(if ($env:JAVA_HOME) { $env:JAVA_HOME } else { 'PATH' })"
Write-Host "SDK:     $androidSdk"
Write-Host "NDK:     $requiredNdkVersion"
Write-Host "Gradle:  $requiredGradleVersion"
Write-Host 'AGP:     9.5.0'
Write-Host 'Package: io.github.kodeaqua.vanillabtn'
Write-Host '=========================================================='

Push-Location $portAndroidDir
try {
    $taskName = "assemble$BuildType"
    & $gradleWrapper $taskName --no-daemon
    if ($LASTEXITCODE -ne 0) {
        throw "Android Gradle build failed with exit code $LASTEXITCODE"
    }

    $apkSubdir = $BuildType.ToLowerInvariant()
    $apkPath = Join-Path $portAndroidDir "app/build/outputs/apk/$apkSubdir/app-$apkSubdir.apk"
    if (-not (Test-Path $apkPath)) {
        $apkPath = Join-Path $portAndroidDir "app/build/outputs/apk/$apkSubdir/app-$apkSubdir-unsigned.apk"
    }

    if (Test-Path $apkPath) {
        $distDir = Join-Path $repoRoot 'dist'
        if (-not (Test-Path $distDir)) {
            New-Item -ItemType Directory -Force -Path $distDir | Out-Null
        }
        $finalApkName = 'VanillaBTN-android-v0.9.0.apk'
        $finalApkPath = Join-Path $distDir $finalApkName
        Copy-Item -Path $apkPath -Destination $finalApkPath -Force

        $fileSizeMB = [math]::Round((Get-Item $finalApkPath).Length / 1MB, 2)
        Write-Host ''
        Write-Host '[ANDROID BUILD SUCCESS]'
        Write-Host "Output APK: $finalApkPath ($fileSizeMB MB)"
        Write-Host '=========================================================='
    } else {
        Write-Warning "Could not find output APK at expected path: $apkPath"
    }
} finally {
    Pop-Location
}
