param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    [string]$OynonToolsRoot = (Join-Path $PSScriptRoot '..\OynonTools'),
    [string]$LuaCompilerRoot = (Join-Path $PSScriptRoot '..\pathologic_lua_compiler'),
    [string]$PathologicReRoot = (Join-Path $PSScriptRoot '..\pathologic_re')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build-win32'
& python (Join-Path $PSScriptRoot 'scripts\prepare_choice_audio.py') --check
if ($LASTEXITCODE -ne 0) { throw 'Bundled choice audio validation failed' }
& python (Join-Path $PSScriptRoot 'scripts\build_history.py') --compiler-root $LuaCompilerRoot --pathologic-re $PathologicReRoot --game-root $GameRoot
if ($LASTEXITCODE -ne 0) { throw 'Feed compilation failed' }
& python (Join-Path $PSScriptRoot 'tests\feed_behavior_test.py') --compiler-root $LuaCompilerRoot
if ($LASTEXITCODE -ne 0) { throw 'Feed behavior checks failed' }
& python (Join-Path $PSScriptRoot 'scripts\generate_layouts.py') --game-root $GameRoot
if ($LASTEXITCODE -ne 0) { throw 'Layout generation failed' }
& cmake -S (Join-Path $PSScriptRoot '..\UtopianInventory') -B (Join-Path $PSScriptRoot 'tmp\build-inventory-stage2') -A Win32 "-DOYNONTOOLS_ROOT=$OynonToolsRoot"
if ($LASTEXITCODE -ne 0) { throw 'Inventory compatibility configure failed' }
& cmake --build (Join-Path $PSScriptRoot 'tmp\build-inventory-stage2') --config Release
if ($LASTEXITCODE -ne 0) { throw 'Inventory compatibility build failed; rebuild OynonTools first' }
& cmake -S $PSScriptRoot -B $build -A Win32 "-DOYNONTOOLS_ROOT=$OynonToolsRoot"
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
& cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
& python (Join-Path $PSScriptRoot 'tests\verify_hd_abi.py') --game-root $GameRoot --pathologic-re $PathologicReRoot
if ($LASTEXITCODE -ne 0) { throw 'HD ABI validation failed' }
& python (Join-Path $PSScriptRoot 'scripts\package_release.py') --oynon-root $OynonToolsRoot --game-root $GameRoot
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
