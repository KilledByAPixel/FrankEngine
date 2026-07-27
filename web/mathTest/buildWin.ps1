# Phase 1 math parity suite - Windows build (cl.exe, x86, legacy DirectX SDK).
# Usage: buildWin.ps1 -config d3dx|compare|frank   (see mathTest.cpp header)
param(
    [ValidateSet("d3dx", "compare", "frank")]
    [string]$config = "d3dx"
)

$ErrorActionPreference = "Stop"

$testDir = $PSScriptRoot
$outDir = Join-Path $testDir "build"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory $outDir | Out-Null }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio with C++ tools not found" }

$defines = switch ($config) {
    "d3dx"    { "/DMATHTEST_USE_D3DX" }
    "compare" { "/DMATHTEST_COMPARE" }
    "frank"   { "/DFRANK_PLATFORM_WEB" }
}
$exe = Join-Path $outDir "mathTest_$config.exe"

# d3dx & compare configs need the legacy DirectX SDK; frank config must NOT touch it,
# but it compiles the real frankMath which needs Box2D headers
$dxInclude = ""
$dxLib = ""
if ($config -ne "frank") {
    if (-not $env:DXSDK_DIR) { throw "DXSDK_DIR not set" }
    $dxInclude = "/I`"$($env:DXSDK_DIR)Include`""
    $dxLib = "/LIBPATH:`"$($env:DXSDK_DIR)Lib\x86`" d3dx9.lib"
} else {
    $engineRoot = Resolve-Path (Join-Path $testDir "..\..")
    $dxInclude = "/I`"$engineRoot\box2d`""
}

# VsDevCmd sets up the x86 cl environment; everything runs inside one cmd instance
$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
$clCmd = "cl /nologo /EHsc /W3 /O2 $defines $dxInclude `"$testDir\mathTest.cpp`" /Fo`"$outDir\\`" /Fe`"$exe`" /link $dxLib"
cmd /c "`"$vsDevCmd`" -arch=x86 -no_logo && $clCmd"

if ($LASTEXITCODE -ne 0) { Write-Host "build FAILED"; exit $LASTEXITCODE }
Write-Host "built $exe"
