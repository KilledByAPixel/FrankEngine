# Phase 1 math parity suite - wasm build (config C: real frankMath, no D3DX).
# Run the result with: c:\dev\tools\emsdk\node\22.16.0_64bit\bin\node.exe build\mathTest.js
$ErrorActionPreference = "Stop"

$testDir = $PSScriptRoot
$engineRoot = Resolve-Path (Join-Path $testDir "..\..")
$outDir = Join-Path $testDir "build"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory $outDir | Out-Null }

# this machine's PATH python is 2.7, which breaks the em++ launcher
$env:EMSDK_PYTHON = "c:\dev\tools\emsdk\python\3.13.3_64bit\python.exe"
$empp = "c:\dev\tools\emsdk\upstream\emscripten\em++.exe"

& $empp (Join-Path $testDir "mathTest.cpp") `
    -I (Join-Path $engineRoot "box2d") `
    -DFRANK_PLATFORM_WEB `
    -std=c++14 -O2 `
    -o (Join-Path $outDir "mathTest.js")

if ($LASTEXITCODE -eq 0) {
    Write-Host "build OK - run with node build\mathTest.js"
} else {
    Write-Host "build FAILED with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
