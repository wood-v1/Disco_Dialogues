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
& cmake -S $PSScriptRoot -B $build -A Win32 "-DOYNONTOOLS_ROOT=$OynonToolsRoot"
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
& cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
# Inventory's standalone build consumes an SDK layout. Supply this build's
# headers/import library/DLL so compatibility artifacts use the same dependency.
$sdk = Join-Path $PSScriptRoot 'tmp\oynontools-sdk'
foreach ($directory in @('include', 'lib\Win32\Release', 'bin\Win32\Release')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $sdk $directory) | Out-Null
}
Copy-Item -Path (Join-Path $OynonToolsRoot 'include\*') -Destination (Join-Path $sdk 'include') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $build 'Release\OynonTools.lib') -Destination (Join-Path $sdk 'lib\Win32\Release\OynonTools.lib') -Force
Copy-Item -LiteralPath (Join-Path $build 'Release\OynonTools.dll') -Destination (Join-Path $sdk 'bin\Win32\Release\OynonTools.dll') -Force
& cmake -S (Join-Path $PSScriptRoot '..\UtopianInventory') -B (Join-Path $PSScriptRoot 'tmp\build-inventory-stage2') -A Win32 "-DOYNONTOOLS_ROOT=$sdk"
if ($LASTEXITCODE -ne 0) { throw 'Inventory compatibility configure failed' }
& cmake --build (Join-Path $PSScriptRoot 'tmp\build-inventory-stage2') --config Release
if ($LASTEXITCODE -ne 0) { throw 'Inventory compatibility build failed' }
& ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
& python (Join-Path $PSScriptRoot 'tests\verify_hd_abi.py') --game-root $GameRoot --pathologic-re $PathologicReRoot --oynon-root $OynonToolsRoot
if ($LASTEXITCODE -ne 0) { throw 'HD ABI validation failed' }
& python (Join-Path $PSScriptRoot 'scripts\package_release.py') --oynon-root $OynonToolsRoot --game-root $GameRoot
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
