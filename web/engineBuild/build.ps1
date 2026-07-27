# Phase 2/3 web engine build - compiles the engine's portable TU list (plus Box2D,
# stubs, web shell, TestGame) under em++ with FRANK_PLATFORM_WEB.
# Usage: build.ps1 [-compileOnly] [-only <name>]   (-only compiles a single TU by substring)
param(
    [switch]$compileOnly,
    [string]$only = ""
)
$ErrorActionPreference = "Continue"

$buildDir = $PSScriptRoot
$engineRoot = Resolve-Path (Join-Path $buildDir "..\..")
$src = Join-Path $engineRoot "FrankEngine\Source"
$outDir = Join-Path $buildDir "build"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory $outDir | Out-Null }

$env:EMSDK_PYTHON = "c:\dev\tools\emsdk\python\3.13.3_64bit\python.exe"
$empp = "c:\dev\tools\emsdk\upstream\emscripten\em++.exe"

# engine TUs compiled for web (renderer/sound/input/gui/editor/shell excluded - stubbed)
$engineSources = @(
    "gameControlBase.cpp",
    "Core\frankMath.cpp",
    "Core\debugConsole.cpp",
    "Core\inputControl.cpp",
    "Core\debugMessage.cpp",
    "Core\frankUtil.cpp",
    "Core\pathFindingBase.cpp",
    "Core\perlinNoise.cpp",
    "Core\frankProfiler.cpp",
    "Objects\gameObject.cpp",
    "Objects\gameObjectManager.cpp",
    "Objects\gameObjectBuilder.cpp",
    "Objects\actor.cpp",
    "Objects\basicObjects.cpp",
    "Objects\camera.cpp",
    "Objects\light.cpp",
    "Objects\particleSystem.cpp",
    "Objects\projectile.cpp",
    "Objects\weapon.cpp",
    "Physics\physics.cpp",
    "Physics\physicsRender.cpp",
    "Rendering\deferredRender.cpp",
    "Rendering\frankFont.cpp",
    "Rendering\miniMap.cpp",
    "Sound\soundControl.cpp",
    "Sound\musicControl.cpp",
    "Terrain\terrain.cpp",
    "Terrain\terrainTile.cpp",
    "Terrain\terrainSurface.cpp"
)

$flags = @(
    "-I", (Join-Path $engineRoot "box2d"),
    "-I", (Join-Path $engineRoot "oggvorbis\ogg\include"),
    "-I", (Join-Path $engineRoot "oggvorbis\vorbis\include"),
    "-I", $src,
    "-DFRANK_PLATFORM_WEB",
    "-std=c++14", "-O1",
    "-Wno-inconsistent-missing-override", "-Wno-writable-strings", "-Wno-dangling-else",
    "-Wno-nonportable-include-path"
)

$failed = 0
$compiled = 0
foreach ($rel in $engineSources) {
    if ($only -and ($rel -notlike "*$only*")) { continue }
    $cpp = Join-Path $src $rel
    $obj = Join-Path $outDir (($rel -replace "[\\\/]", "_") -replace "\.cpp$", ".o")
    & $empp -c $cpp @flags -o $obj
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $rel"; $failed++ } else { $compiled++ }
}

Write-Host "engine TUs: $compiled compiled, $failed failed"
if ($failed -gt 0) { exit 1 }
if ($compileOnly -or $only) { exit 0 }

# vendored oggvorbis (decode path) compiled as C with its own flags, cached as objects;
# barkmel/psytune/tone are standalone tools with main()
$oggCSources = @()
$oggCSources += Get-ChildItem (Join-Path $engineRoot "oggvorbis\ogg\src") -Filter *.c | ForEach-Object { $_.FullName }
$oggCSources += Get-ChildItem (Join-Path $engineRoot "oggvorbis\vorbis\lib") -Filter *.c |
    Where-Object { $_.Name -notin @("barkmel.c", "psytune.c", "tone.c", "vorbisenc.c") } | ForEach-Object { $_.FullName }
$oggFlags = @(
    "-I", (Join-Path $engineRoot "oggvorbis\ogg\include"),
    "-I", (Join-Path $engineRoot "oggvorbis\vorbis\include"),
    "-O2", "-w"
)
$oggObjs = @()
foreach ($c in $oggCSources) {
    $obj = Join-Path $outDir ("ogg_" + [IO.Path]::GetFileNameWithoutExtension($c) + ".o")
    $oggObjs += $obj
    if ((Test-Path $obj) -and ((Get-Item $obj).LastWriteTime -gt (Get-Item $c).LastWriteTime)) { continue }
    & $empp -c -x c $c @oggFlags -o $obj
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $c"; exit 1 }
}

# link step (T4+): Box2D + oggvorbis + engine objects + stubs + web shell + TestGame
$box2dSources = Get-ChildItem (Join-Path $engineRoot "box2d\Box2D") -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
$webSources = @(
    (Join-Path $src "Web\webStubs.cpp"),
    (Join-Path $src "Web\webRender.cpp"),
    (Join-Path $src "Web\webGui.cpp"),
    (Join-Path $src "Web\webSound.cpp"),
    (Join-Path $src "Web\frankEngineWeb.cpp")
)
# gameGUI.cpp is DXUT-dialog based; the web build compiles gameGUIWeb.cpp instead
$testGameSources = Get-ChildItem (Join-Path $engineRoot "TestGame\Source") -Recurse -Filter *.cpp |
    Where-Object { $_.Name -ne "gameGUI.cpp" } | ForEach-Object { $_.FullName }

$objs = Get-ChildItem $outDir -Filter *.o | ForEach-Object { $_.FullName }
$linkInputs = @($objs) + $box2dSources + $webSources + $testGameSources

& $empp @linkInputs @flags `
    -I (Join-Path $engineRoot "TestGame\Source") `
    -I (Join-Path $engineRoot "TestGame\Source\objects") `
    -sALLOW_MEMORY_GROWTH=1 `
    -sEXIT_RUNTIME=0 `
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 `
    -lopenal `
    -sSTACK_SIZE=4MB `
    "-Wl,--error-limit=0" `
    --preload-file ((Join-Path $engineRoot "TestGame\terrain.2dt") + "@/terrain.2dt") `
    --preload-file ((Join-Path $engineRoot "TestGame\autoexec.cfg") + "@/autoexec.cfg") `
    --preload-file ((Join-Path $engineRoot "TestGame\testMusic.ogg") + "@/testMusic.ogg") `
    --preload-file ((Join-Path $engineRoot "TestGame\data") + "@/data") `
    --use-preload-plugins `
    -o (Join-Path $outDir "testGame.js")

if ($LASTEXITCODE -eq 0) {
    $wasm = Get-Item (Join-Path $outDir "testGame.wasm")
    Write-Host ("link OK - testGame.wasm is {0:N0} bytes" -f $wasm.Length)
} else {
    Write-Host "link FAILED"
    exit 1
}
