# Phase 0 spike build - compiles Box2D + spikeMain.cpp to WASM with em++.
# Throwaway scaffolding; the real engine web build (Phase 2+) replaces this.
# Prereq: emsdk installed at c:\dev\tools\emsdk (see docs/plans/2026-07-21-piroot-web-demo.md)

$ErrorActionPreference = "Stop"

$spikeDir = $PSScriptRoot
$engineRoot = Resolve-Path (Join-Path $spikeDir "..\..")
$box2dRoot = Join-Path $engineRoot "box2d"
$outDir = Join-Path $spikeDir "build"

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory $outDir | Out-Null }

# box2d\Box2D\Box2D.cpp is a header-only stub; globbing it in is harmless
$box2dSources = Get-ChildItem (Join-Path $box2dRoot "Box2D") -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
$sources = @((Join-Path $spikeDir "spikeMain.cpp")) + $box2dSources

# This machine's PATH python is 2.7, which breaks the em++ launcher - force the SDK's bundled python
$env:EMSDK_PYTHON = "c:\dev\tools\emsdk\python\3.13.3_64bit\python.exe"
$empp = "c:\dev\tools\emsdk\upstream\emscripten\em++.exe"

& $empp @sources `
    -I $box2dRoot `
    -std=c++14 -O2 `
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 `
    -sALLOW_MEMORY_GROWTH=1 `
    -o (Join-Path $outDir "spike.html")

if ($LASTEXITCODE -eq 0) {
    $wasm = Get-Item (Join-Path $outDir "spike.wasm")
    Write-Host ("build OK - spike.wasm is {0:N0} bytes" -f $wasm.Length)
} else {
    Write-Host "build FAILED with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
