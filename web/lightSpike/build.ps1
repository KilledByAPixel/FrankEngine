# Phase 1.5 lighting spike build - deferred lighting shaders in WebGL2.
$ErrorActionPreference = "Stop"

$spikeDir = $PSScriptRoot
$engineRoot = Resolve-Path (Join-Path $spikeDir "..\..")
$outDir = Join-Path $spikeDir "build"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory $outDir | Out-Null }

# this machine's PATH python is 2.7, which breaks the em++ launcher
$env:EMSDK_PYTHON = "c:\dev\tools\emsdk\python\3.13.3_64bit\python.exe"
$empp = "c:\dev\tools\emsdk\upstream\emscripten\em++.exe"

& $empp (Join-Path $spikeDir "lightSpike.cpp") `
    -I (Join-Path $engineRoot "FrankEngine\Source\Core") `
    -std=c++14 -O2 `
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 `
    -sALLOW_MEMORY_GROWTH=1 `
    -o (Join-Path $outDir "lightSpike.js")

if ($LASTEXITCODE -eq 0) {
    $wasm = Get-Item (Join-Path $outDir "lightSpike.wasm")
    Write-Host ("build OK - lightSpike.wasm is {0:N0} bytes" -f $wasm.Length)
} else {
    Write-Host "build FAILED with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
