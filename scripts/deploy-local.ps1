param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    [string]$PreviousManifest = 'Disco_Dialogues_0.2.0'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path $PSScriptRoot -Parent
$final = Join-Path $gameRoot 'bin\Final'
$launcher = Join-Path $final 'GameModLauncher.exe'
$package = Join-Path $project 'release\Pathologic_Disco_Dialogues_0_2_1.zip'
$report = Get-Content (Join-Path $project 'release\validation.json') -Raw | ConvertFrom-Json
if (Get-Process Game -ErrorAction SilentlyContinue) { throw 'Close Game.exe before deployment.' }
if ((Get-FileHash $package).Hash -ne $report.sha256) { throw 'Package hash mismatch.' }
if ($PreviousManifest -notmatch '^[A-Za-z0-9_.]+$') { throw 'Invalid previous package ID.' }
$manifestRelative = 'bin\Final\mods\.launcher\' + $PreviousManifest + '.install.txt'
$manifest = Join-Path $gameRoot $manifestRelative
$owned = @(Get-Content -LiteralPath $manifest | ForEach-Object {
    $fields = $_ -split "`t"
    if ($fields[1] -ne 'created' -or $fields[0] -notmatch '^(bin\\Final\\mods\\DiscoDialogues\.(dll|ini|manifest\.ini)|data\\Scripts\\disco_dialogues_(history|feed)\.bin|data\\UI\\disco_dialogues_(1366x768|1600x900|1920x1080)\.xml)$') {
        throw 'Unexpected old package ownership; refusing uninstall.'
    }
    $fields[0]
})
if ($owned.Count -ne 7) { throw 'Unexpected old package file count.' }
$backup = Join-Path $project ('tmp\deploy-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
$saved = @($owned) + @('bin\Final\GameModLauncher.ini', $manifestRelative, 'bin\Final\mods\.launcher\shared-OynonTools.dll.install.txt')
foreach ($relative in $saved) {
    $destination = Join-Path $backup $relative
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $gameRoot $relative) -Destination $destination
}
$protected = @(Get-ChildItem -LiteralPath (Join-Path $final 'mods') -File | Where-Object { $_.Name -match '^(InventoryOverhaul\.|PPMM\.|OynonTools\.dll$)' })
$protected += @(Get-ChildItem -LiteralPath (Join-Path $gameRoot 'data') -Recurse -File | Where-Object { $_.Name -like 'inv_overhaul_*' -or $_.Extension -eq '.vfs' -or $_.Name -eq 'main.dat' })
$before = @{}
foreach ($file in $protected) { $before[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName).Hash }
$before | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'protected-hashes.json') -Encoding UTF8
$ini = Join-Path $final 'GameModLauncher.ini'
$oldOrder = (Get-Content -LiteralPath $ini | Where-Object { $_ -like 'LoadOrder=*' })
Push-Location $final
try {
    & $launcher delete --mod DiscoDialogues.dll
    if ($LASTEXITCODE -ne 0) { throw 'Launcher uninstall failed; backup is preserved.' }
    & $launcher install --zip $package --name 'Disco Dialogues 0.2.1' --dll DiscoDialogues.dll --skip-dll OynonTools.dll
    if ($LASTEXITCODE -ne 0) { throw 'Launcher install failed; backup is preserved.' }
    Copy-Item -LiteralPath (Join-Path $backup 'bin\Final\mods\DiscoDialogues.ini') -Destination (Join-Path $final 'mods\DiscoDialogues.ini') -Force
    $oldTokens = @(($oldOrder -replace '^LoadOrder=', '') -split ',\s*')
    $desiredIndex = [Array]::FindIndex($oldTokens, [Predicate[string]] { param($token) $token -like 'DiscoDialogues.dll@*' })
    for ($attempt = 0; $attempt -lt $oldTokens.Count; ++$attempt) {
        $newOrder = (Get-Content -LiteralPath $ini | Where-Object { $_ -like 'LoadOrder=*' })
        if ($newOrder -eq $oldOrder) { break }
        $tokens = @(($newOrder -replace '^LoadOrder=', '') -split ',\s*')
        $currentIndex = [Array]::FindIndex($tokens, [Predicate[string]] { param($token) $token -like 'DiscoDialogues.dll@*' })
        if ($currentIndex -lt 0 -or $desiredIndex -lt 0) { throw 'Cannot locate mod in load order.' }
        $direction = if ($currentIndex -lt $desiredIndex) { '--down' } else { '--up' }
        & $launcher move --mod DiscoDialogues.dll $direction
        if ($LASTEXITCODE -ne 0) { throw 'Could not restore load order.' }
    }
} finally { Pop-Location }
if ((Get-Content -LiteralPath $ini | Where-Object { $_ -like 'LoadOrder=*' }) -ne $oldOrder) { throw 'Load order differs; inspect backup.' }
$verified = @()
foreach ($entry in $report.files.PSObject.Properties) {
    if ($entry.Name -in @('bin/Final/GameModLauncher.ini', 'bin/Final/mods/DiscoDialogues.ini')) { continue }
    $target = Join-Path $gameRoot $entry.Name
    if ((Get-FileHash -LiteralPath $target).Hash -ne $entry.Value) { throw "Installed hash mismatch: $target" }
    $verified += $entry.Name
}
foreach ($path in $before.Keys) {
    if ((Get-FileHash -LiteralPath $path).Hash -ne $before[$path]) { throw "Protected file changed: $path" }
}
if ((Get-FileHash (Join-Path $final 'mods\DiscoDialogues.ini')).Hash -ne (Get-FileHash (Join-Path $backup 'bin\Final\mods\DiscoDialogues.ini')).Hash) { throw 'User mod settings changed.' }
@{ status='PASS'; game_root=$gameRoot; backup=$backup; installed_version='0.2.1'; verified_files=$verified; protected_files_unchanged=$before.Count; load_order_preserved=$true; mod_settings_preserved=$true; gameplay_test='NOT RUN' } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $project 'release\deployment-validation.json') -Encoding UTF8
Write-Output "PASS: deployed 0.2.1; $($verified.Count) hashes verified; $($before.Count) protected files unchanged. Backup: $backup"
