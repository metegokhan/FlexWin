param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$distDir = Join-Path $root "dist"
$build64 = Join-Path $root "build-x64"
$build32 = Join-Path $root "build-x86"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Visual Studio C++ x64/x86 Build Tools bulunamadi."
}

$cmakeExe = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmakeExe)) {
    $cmakeExe = "cmake"
}

Write-Host "==> Configuring x64 targets..." -ForegroundColor Cyan
& $cmakeExe -S $root -B $build64 -G "Visual Studio 17 2022" -A x64 -DFLEXWIN_ARCH=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building x64 targets ($Config)..." -ForegroundColor Cyan
& $cmakeExe --build $build64 --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Configuring x86 targets..." -ForegroundColor Cyan
& $cmakeExe -S $root -B $build32 -G "Visual Studio 17 2022" -A Win32 -DFLEXWIN_ARCH=x86
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building x86 targets ($Config)..." -ForegroundColor Cyan
& $cmakeExe --build $build32 --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$x64Bin = Join-Path $build64 $Config
$x32Bin = Join-Path $build32 $Config

function Copy-FileReplacingLocked([string]$Source, [string]$DestDir) {
    $name = Split-Path -Leaf $Source
    $dest = Join-Path $DestDir $name
    $stale = "$dest.old"
    if (Test-Path $stale) {
        Remove-Item $stale -Force -ErrorAction SilentlyContinue
    }
    try {
        Copy-Item $Source $dest -Force -ErrorAction Stop
    } catch {
        if (Test-Path $dest) {
            $tmpOld = "$dest.old_" + ([guid]::NewGuid().ToString("N").Substring(0, 6))
            Rename-Item -Path $dest -NewName (Split-Path -Leaf $tmpOld) -Force
            Copy-Item $Source $dest -Force
        } else {
            throw
        }
    }
}

Get-ChildItem $distDir -Filter "*.old*" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

Copy-FileReplacingLocked (Join-Path $x64Bin "FlexWin.exe") $distDir
Copy-FileReplacingLocked (Join-Path $x64Bin "FlexWinHook64.dll") $distDir
Copy-FileReplacingLocked (Join-Path $x32Bin "FlexWinHelper32.exe") $distDir
Copy-FileReplacingLocked (Join-Path $x32Bin "FlexWinHook32.dll") $distDir

Write-Host "==> Portable dist/ folder ready:" -ForegroundColor Green
Get-ChildItem $distDir | Format-Table Name, Length -AutoSize

if ($RunTests) {
    $testDir = Join-Path $root "build-tests"
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null
    Copy-Item (Join-Path $x64Bin "FlexWin.exe") $testDir -Force
    Copy-Item (Join-Path $x64Bin "FlexWinHook64.dll") $testDir -Force
    Copy-Item (Join-Path $x32Bin "FlexWinHelper32.exe") $testDir -Force
    Copy-Item (Join-Path $x32Bin "FlexWinHook32.dll") $testDir -Force
    Copy-Item (Join-Path $x64Bin "FlexWinTests.exe") $testDir -Force
    Copy-Item (Join-Path $x64Bin "FlexWinIntegrationTest.exe") $testDir -Force
    Copy-Item (Join-Path $x64Bin "StubbornTestApp64.exe") $testDir -Force
    Copy-Item (Join-Path $x32Bin "StubbornTestApp32.exe") $testDir -Force

    Write-Host "==> Running Unit Tests..." -ForegroundColor Cyan
    & (Join-Path $testDir "FlexWinTests.exe")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "==> Running Live x64 & x86 Hook Integration Tests..." -ForegroundColor Cyan
    & (Join-Path $testDir "FlexWinIntegrationTest.exe")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "==> ALL TESTS PASSED!" -ForegroundColor Green
}
