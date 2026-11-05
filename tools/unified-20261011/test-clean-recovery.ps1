$ErrorActionPreference='Stop'
. "$PSScriptRoot/DLSS5-Clean-Recovery.ps1"
$testRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot ('../../builds/unified-20261011/clean-fixture-'+[guid]::NewGuid().ToString('N'))))
Assert-CleanPath $testRoot
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$script:checks=0
$script:running=$false
$script:driverCode=0
$script:failRegistry=$false
function Assert-CleanClosed { if($script:running) { throw 'running fixture' } }
function Get-CleanRegistrations { foreach($entry in $script:registry) { [pscustomobject]@{hive=$entry.hive;view=$entry.view;key=$entry.key;name=$entry.name;value=$entry.value} } }
function Set-CleanRegistration($Entry,[int]$Expected,[int]$Value) {
    if($script:failRegistry) { throw 'injected registry failure' }
    $target=@($script:registry | Where-Object name -eq $Entry.name)[0]
    if($target.value -ne $Expected) { throw 'registry fixture changed' }
    $target.value=$Value
}
function Invoke-CleanDriver([string]$Game) { return $script:driverCode }
function Put([string]$Path,[string]$Text) { [IO.Directory]::CreateDirectory((Split-Path -Path $Path -Parent)) | Out-Null; [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false)) }
function Check([bool]$Value,[string]$Message) { if(!$Value) { throw $Message }; $script:checks++ }
function Reject([scriptblock]$Work,[string]$Message) { $failed=$false; try { & $Work } catch { $failed=$true }; Check $failed $Message }
function Fixture([string]$Name) {
    $game=Join-Path $testRoot ($Name+' 中文 游戏 & O''Brien')
    foreach($name in @('Endfield.exe','sl.interposer.dll','sl.dlss.dll','unknown.addon64','unrelated-save.dat',
        'dlss5-bridge.addon64','renodx-dlss5.addon64','renodx-mfgunlock.addon64','ReShade.ini',
        'dlss5-bridge.cfg','vk-present-adapter.ini','dlss5/state/bridge.state','dlss5/nr/runtime.dll','reshade-shaders/Shaders/demo.fx')) { Put (Join-Path $game $name) "fixture $name" }
    $layer=Join-Path $testRoot ($Name+'-ReShade/ReShade64.json')
    Put $layer '{"layer":{"name":"VK_LAYER_reshade"}}'
    Put (Join-Path (Split-Path $layer -Parent) 'ReShadeApps.ini') ('Apps='+$game+'\Endfield.exe')
    $script:registry=@([pscustomobject]@{hive='LocalMachine';view='Registry64';key='mock';name=$layer;value=0})
    return $game
}
$game=Fixture 'normal'
$plan=Get-CleanPlan $game
Check ($plan.shared_apps.Count -eq 0) 'single-game scope'
Check ($plan.files.Count -eq 9) 'only owned files selected'
$backup=Join-Path $testRoot 'archive-normal'
Invoke-CleanRestore $plan $backup $false
Check ($script:registry[0].value -eq 1) 'layer disabled'
foreach($entry in $plan.files) { Check (!(Test-Path -LiteralPath (Join-Path $game $entry.path))) 'component not removed'; Check ((Clean-Hash (Join-Path $backup ('files/'+$entry.path))) -eq $entry.hash) 'backup integrity' }
foreach($leaf in @('Endfield.exe','sl.interposer.dll','sl.dlss.dll','unknown.addon64','unrelated-save.dat')) { Check ([IO.File]::ReadAllText((Join-Path $game $leaf)) -eq "fixture $leaf") 'unrelated data changed' }
Check ((Get-CleanPlan $game).files.Count -eq 0) 'remaining active components'
Invoke-CleanUndo $backup $game
Check ($script:registry[0].value -eq 0) 'registration restored'
foreach($entry in $plan.files) { Check ((Clean-Hash (Join-Path $game $entry.path)) -eq $entry.hash) 'undo integrity' }
$game=Fixture 'blocked'
$plan=Get-CleanPlan $game
$script:running=$true
Reject { Invoke-CleanRestore $plan (Join-Path $testRoot 'running-archive') $false } 'running process must reject'
$script:running=$false
Check (!(Test-Path -LiteralPath (Join-Path $testRoot 'running-archive'))) 'blocked operation wrote backup'
Put (Join-Path $game 'dlss5-bridge.addon64') 'changed after plan'
Reject { Invoke-CleanRestore $plan (Join-Path $testRoot 'changed-archive') $false } 'changed source must reject'
Check ($script:registry[0].value -eq 0) 'changed source altered registry'
$plan=Get-CleanPlan $game
Reject { Invoke-CleanRestore $plan (Join-Path $game 'backup') $false } 'backup cannot be inside game'
Reject { Clean-Child $game '../escape' } 'relative escape'
$game=Fixture 'shared'
$other=Join-Path $testRoot 'other-game\Game.exe'
Put $other 'fixture other game'
Put (Join-Path (Split-Path $script:registry[0].name -Parent) 'ReShadeApps.ini') ('Apps='+$game+'\Endfield.exe,'+$other)
$plan=Get-CleanPlan $game
Check ($plan.shared_apps.Count -eq 1 -and $plan.shared_apps[0] -eq $other) 'other game must be detected'
Reject { Invoke-CleanRestore $plan (Join-Path $testRoot 'shared-blocked') $false } 'shared registration requires explicit scope'
$script:driverCode=4
$backup=Join-Path $testRoot 'shared-accepted'
Invoke-CleanRestore $plan $backup $true
Check (!(Test-Path -LiteralPath (Join-Path $game 'dlss5-bridge.addon64'))) 'DRS failure blocked renderer removal'
$result=Get-Content -LiteralPath (Join-Path $backup 'result.json') -Raw | ConvertFrom-Json
Check ($result.driver_exit -eq 4 -and $result.status -match 'launcher-repair-required') 'incomplete result hidden'
Put (Join-Path $game 'ReShade.ini') 'user changed configuration'
Reject { Invoke-CleanUndo $backup $game } 'undo must keep new configuration'
Check ([IO.File]::ReadAllText((Join-Path $game 'ReShade.ini')) -eq 'user changed configuration') 'undo overwrote user changes'
$script:driverCode=0
$game=Fixture 'stale'
$stale=Join-Path $testRoot 'moved-away\Old-Game.exe'
Put (Join-Path (Split-Path $script:registry[0].name -Parent) 'ReShadeApps.ini') ('Apps='+$game+'\Endfield.exe,'+$stale)
$plan=Get-CleanPlan $game
Check ($plan.shared_apps.Count -eq 0) 'nonexistent program path counted as shared'
Check ($plan.stale_apps.Count -eq 1 -and $plan.stale_apps[0] -eq $stale) 'nonexistent program path not recorded as stale'
Invoke-CleanRestore $plan (Join-Path $testRoot 'stale-archive') $false
Check (!(Test-Path -LiteralPath (Join-Path $game 'dlss5-bridge.addon64'))) 'stale program path blocked removal'
$game=Fixture 'normalized'
$alias=(($game -replace '\\','/')+'/endfield.exe')
$alias=$alias.Substring(0,1).ToLowerInvariant()+$alias.Substring(1)
Put (Join-Path (Split-Path $script:registry[0].name -Parent) 'ReShadeApps.ini') ('Apps='+$alias)
$plan=Get-CleanPlan $game
Check ($plan.shared_apps.Count -eq 0) 'current game in alternate notation counted as shared'
Check ($plan.stale_apps.Count -eq 0) 'current game in alternate notation counted as stale'
$game=Fixture 'no-scope-ini'
$orphanLayer=Join-Path $testRoot 'no-scope-ini-ReShade\ReShade64.json'
Put $orphanLayer '{"layer":{"name":"VK_LAYER_reshade"}}'
$script:registry=@([pscustomobject]@{hive='LocalMachine';view='Registry64';key='mock';name=$orphanLayer;value=0})
$plan=Get-CleanPlan $game
Check ($plan.shared_apps.Count -eq 1 -and $plan.shared_apps[0] -match 'Unknown application scope') 'missing scope manifest not flagged'
Reject { Invoke-CleanRestore $plan (Join-Path $testRoot 'no-scope-archive') $false } 'unknown scope must still require confirmation'
$game=Fixture 'partial'
$plan=Get-CleanPlan $game
$script:failRegistry=$true
$backup=Join-Path $testRoot 'partial-archive'
Reject { Invoke-CleanRestore $plan $backup $false } 'registry failure not reported'
$script:failRegistry=$false
Invoke-CleanUndo $backup $game
Check ($script:registry[0].value -eq 0) 'partial undo changed original registration'
foreach($entry in $plan.files) { Check ((Clean-Hash (Join-Path $game $entry.path)) -eq $entry.hash) 'partial undo integrity' }
$game=Fixture 'link'
$outside=Join-Path $testRoot 'outside'; [IO.Directory]::CreateDirectory($outside) | Out-Null
New-Item -ItemType Junction -Path (Join-Path $game 'dlss5/escape') -Target $outside | Out-Null
Reject { Get-CleanPlan $game } 'linked child must reject'
$game=Fixture 'move-interrupted'
$plan=Get-CleanPlan $game
$backup=Join-Path $testRoot 'interrupted-archive'
$locked=[IO.File]::Open((Join-Path $game 'renodx-mfgunlock.addon64'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try { Reject { Invoke-CleanRestore $plan $backup $false } 'locked file must interrupt archive' } finally { $locked.Dispose() }
Check (!(Test-Path -LiteralPath (Join-Path $game 'dlss5-bridge.addon64'))) 'test did not reach partial archive'
Invoke-CleanUndo $backup $game
foreach($entry in $plan.files) { Check ((Clean-Hash (Join-Path $game $entry.path)) -eq $entry.hash) 'interrupted archive undo integrity' }
$game=Fixture 'tampered'
$plan=Get-CleanPlan $game
$backup=Join-Path $testRoot 'tampered-archive'
Invoke-CleanRestore $plan $backup $false
Put (Join-Path $backup 'removed/dlss5-bridge.addon64') 'modified archive'
Reject { Invoke-CleanUndo $backup $game } 'tampered archive must reject'
Reject { Invoke-CleanUndo $backup $testRoot } 'backup cannot select another game directory'
Check (!(Test-Path -LiteralPath (Join-Path $game 'ReShade.ini'))) 'tampered archive undo mutated game'
Check ($script:registry[0].value -eq 1) 'tampered archive undo re-enabled layer'
# A successful driver restore updates both journal generations. Undo must
# preserve the post-restore records rather than resurrect pending ownership.
$game=Fixture 'fg-journal'
Put (Join-Path $game 'dlss5/state/drs-journal.state') 'legacy pending'
Put (Join-Path $game 'dlss5/state/drs-fg.state') 'FG pending'
function Invoke-CleanDriver([string]$Game) {
    Put (Join-Path $Game 'dlss5/state/drs-journal.state') 'legacy resolved'
    Put (Join-Path $Game 'dlss5/state/drs-fg.state') 'FG resolved'
    return 0
}
$plan=Get-CleanPlan $game
$backup=Join-Path $testRoot 'fg-journal-archive'
Invoke-CleanRestore $plan $backup $false
Invoke-CleanUndo $backup $game
foreach($journal in @('drs-journal.state','drs-fg.state')) {
    Check ([IO.File]::ReadAllText((Join-Path $game ('dlss5/state/'+$journal))) -match 'resolved$') 'undo revived pending DRS ownership'
    Check ([IO.File]::ReadAllText((Join-Path $backup ('files/dlss5/state/'+$journal))) -match 'pending$') 'original DRS evidence changed'
}
# The bridge's startup note and the legacy root trace files are pack files.
$game=Fixture 'root-notes'
foreach($name in @('DLSS5-上次启动异常-说明.txt','RenoDX-DLSS5-normtrace.csv','RenoDX-DLSS5-edittrace.csv')) { Put (Join-Path $game $name) "note $name" }
$plan=Get-CleanPlan $game
foreach($name in @('DLSS5-上次启动异常-说明.txt','RenoDX-DLSS5-normtrace.csv','RenoDX-DLSS5-edittrace.csv')) { Check (@($plan.files | Where-Object path -eq $name).Count -eq 1) "root note not planned: $name" }
# The helper recreates game\dlss5\state and writes records the plan never saw;
# a complete restore archives them and removes the emptied pack folders.
function Invoke-CleanDriver([string]$Game) {
    Put (Join-Path $Game 'dlss5/state/vram-budget-change.state') "helper $($script:driverCode)"
    return $script:driverCode
}
$script:driverCode=0
$game=Fixture 'leftover'
$plan=Get-CleanPlan $game
$backup=Join-Path $testRoot 'leftover-archive'
Invoke-CleanRestore $plan $backup $false
foreach($root in @('dlss5','reshade-shaders')) { Check (!(Test-Path -LiteralPath (Join-Path $game $root))) "pack folder left in the game folder: $root" }
Check ([IO.File]::ReadAllText((Join-Path $backup 'leftover/dlss5/state/vram-budget-change.state')) -eq 'helper 0') 'helper record not archived'
$result=Get-Content -LiteralPath (Join-Path $backup 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Check (@($result.leftovers_archived).Count -eq 1 -and @($result.leftovers_left).Count -eq 0) 'archived leftovers not recorded'
Check ($script:CleanRestoreDriverExit -eq 0) 'complete restore reported as partial'
Invoke-CleanUndo $backup $game
foreach($entry in $plan.files) { Check ((Clean-Hash (Join-Path $game $entry.path)) -eq $entry.hash) 'undo after folder removal' }
$game=Fixture 'leftover-retry'
$script:driverCode=6
Invoke-CleanRestore (Get-CleanPlan $game) (Join-Path $testRoot 'leftover-retry-first') $false
Check ($script:CleanRestoreDriverExit -eq 6) 'partial restore not reported'
Check (Test-Path -LiteralPath (Join-Path $game 'dlss5/state/vram-budget-change.state')) 'helper record removed before the retry'
$script:driverCode=0
$plan=Get-CleanPlan $game
Check (@($plan.files | Where-Object path -match 'vram-budget-change').Count -eq 1) 'retry plan misses the helper record'
Invoke-CleanRestore $plan (Join-Path $testRoot 'leftover-retry-second') $false
Check (!(Test-Path -LiteralPath (Join-Path $game 'dlss5'))) 'retry left the state folder'
foreach($leaf in @('Endfield.exe','sl.interposer.dll','unknown.addon64','unrelated-save.dat')) { Check (Test-Path -LiteralPath (Join-Path $game $leaf)) 'leftover cleanup touched unrelated files' }
# The uninstaller's own files leave only after a complete restore; a failed
# driver or VRAM budget restore keeps them so the user can simply run it again.
$toolNames=@('DLSS5-Recovery 故障恢复.cmd','DLSS5-Clean-Recovery.ps1','DLSS5-RestoreDriver.exe',
             'DLSS5-使用与恢复 Usage and Recovery.md','DLSS5-Clean-Recovery-result.txt')
function Invoke-CleanDriver([string]$Game) { return $script:driverCode }
$script:driverCode=6
$game=Fixture 'tools-retry'
foreach($name in $toolNames) { Put (Join-Path $game $name) "tool $name" }
$backup=Join-Path $testRoot 'tools-retry-first'
Invoke-CleanRestore (Get-CleanPlan $game) $backup $false
foreach($name in $toolNames) { Check (Test-Path -LiteralPath (Join-Path $game $name)) 'uninstaller archived despite an incomplete restore' }
$result=Get-Content -LiteralPath (Join-Path $backup 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Check (@($result.tools_archived).Count -eq 0 -and $result.driver_exit -eq 6) 'incomplete restore reported tools as archived'
$script:driverCode=0
$plan=Get-CleanPlan $game
Check ($plan.files.Count -eq 0) 'retry plan still lists removed components'
$backup=Join-Path $testRoot 'tools-retry-second'
Invoke-CleanRestore $plan $backup $false
foreach($name in $toolNames) {
    Check (!(Test-Path -LiteralPath (Join-Path $game $name))) 'uninstaller file left after a complete restore'
    Check ([IO.File]::ReadAllText((Join-Path $backup ('tools/'+$name))) -eq "tool $name") 'uninstaller archive integrity'
}
$result=Get-Content -LiteralPath (Join-Path $backup 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Check (@($result.tools_archived).Count -eq $toolNames.Count -and @($result.tools_left).Count -eq 0) 'archived tools not recorded'
foreach($leaf in @('Endfield.exe','sl.interposer.dll','unknown.addon64','unrelated-save.dat')) { Check (Test-Path -LiteralPath (Join-Path $game $leaf)) 'tool cleanup touched unrelated files' }
$game=Fixture 'tools-locked'
foreach($name in $toolNames) { Put (Join-Path $game $name) "tool $name" }
$backup=Join-Path $testRoot 'tools-locked-archive'
$locked=[IO.File]::Open((Join-Path $game 'DLSS5-Recovery 故障恢复.cmd'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try { Invoke-CleanRestore (Get-CleanPlan $game) $backup $false } finally { $locked.Dispose() }
Check (!(Test-Path -LiteralPath (Join-Path $game 'dlss5-bridge.addon64'))) 'locked uninstaller blocked component removal'
Check (Test-Path -LiteralPath (Join-Path $game 'DLSS5-Recovery 故障恢复.cmd')) 'locked uninstaller file reported as moved'
Check (!(Test-Path -LiteralPath (Join-Path $game 'DLSS5-Clean-Recovery.ps1'))) 'one locked file stopped the other tool moves'
$result=Get-Content -LiteralPath (Join-Path $backup 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Check (@($result.tools_left).Count -eq 1 -and @($result.tools_left)[0] -eq 'DLSS5-Recovery 故障恢复.cmd') 'locked uninstaller file not reported'
# Interactive Uninstall: confirmation, shared-ReShade second confirmation,
# cancellation and running-game refusal, with the elevated child mocked.
function Read-Host([string]$Prompt) {
    if(!$script:answers.Count) { return '' }
    $answer=$script:answers[0]; $script:answers=@($script:answers | Select-Object -Skip 1); return $answer
}
function Invoke-CleanChild([string]$Game,[bool]$AllowShared) {
    $script:childCalls+=@($AllowShared); return $script:childCodes[$script:childCalls.Count-1]
}
function Uninstall-Case([string[]]$Answers,[int[]]$Codes) {
    $script:answers=$Answers; $script:childCodes=$Codes; $script:childCalls=@()
    return (Invoke-CleanUninstall $game 6>$null)
}
$game=Fixture 'uninstall'
Check ((Uninstall-Case @('n') @()) -eq 0 -and $script:childCalls.Count -eq 0) 'declined uninstall still ran'
Check ((Uninstall-Case @('') @()) -eq 0 -and $script:childCalls.Count -eq 0) 'empty answer ran the uninstall'
Check ((Uninstall-Case @('y') @(0)) -eq 0 -and $script:childCalls.Count -eq 1 -and !$script:childCalls[0]) 'confirmed uninstall did not run once without shared scope'
Check ((Uninstall-Case @('Y','y') @(3,0)) -eq 0 -and $script:childCalls.Count -eq 2 -and $script:childCalls[1]) 'shared ReShade confirmation did not rerun with explicit scope'
Check ((Uninstall-Case @('y','') @(3)) -eq 0 -and $script:childCalls.Count -eq 1) 'declined shared removal still ran'
Check ((Uninstall-Case @('y') @(7)) -eq 7) 'UAC refusal not returned'
Check ((Uninstall-Case @('y') @(1)) -eq 1) 'failure not returned'
Check ((Uninstall-Case @('y') @(9)) -eq 9 -and $script:childCalls.Count -eq 1) 'partial restore not returned'
$script:running=$true
Check ((Uninstall-Case @('y') @(0)) -eq 3 -and $script:childCalls.Count -eq 0) 'running game did not stop the uninstall'
$script:running=$false
Remove-Item Function:\Read-Host
# The elevated Restore reaches the user only through '*> <report>', so the
# helper's own messages must land in that report and must not pollute the
# returned exit code. A compiled stub stands in for DLSS5-RestoreDriver.exe.
$helperDir=Join-Path $testRoot 'helper-output'
[IO.Directory]::CreateDirectory($helperDir) | Out-Null
[IO.File]::Copy((Join-Path $PSScriptRoot 'DLSS5-Clean-Recovery.ps1'),(Join-Path $helperDir 'DLSS5-Clean-Recovery.ps1'),$false)
Add-Type -TypeDefinition 'public static class StubHelper { public static int Main(string[] a) { System.Console.WriteLine("STUB-HELPER-MESSAGE " + a[0]); return 4; } }' -OutputAssembly (Join-Path $helperDir 'DLSS5-RestoreDriver.exe') -OutputType ConsoleApplication
$helperReport=Join-Path $helperDir 'report.txt'
$helperGame=Join-Path $testRoot 'helper-game'
$helperScript=". '$(Join-Path $helperDir 'DLSS5-Clean-Recovery.ps1')'; & { `$script:rc=Invoke-CleanDriver '$helperGame' } *> '$helperReport'; exit `$script:rc"
& (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -Command $helperScript | Out-Null
Check ($LASTEXITCODE -eq 4) "helper exit code not returned cleanly: $LASTEXITCODE"
Check ((Get-Content -LiteralPath $helperReport -Raw) -match 'STUB-HELPER-MESSAGE ') 'helper messages missing from the redirected report'
# Exercise the actual -File entry point without -Game, as used by the CMD.
# Plan only reads real registrations; all file inputs remain in this fixture.
$entry=Join-Path $game 'DLSS5-Clean-Recovery.ps1'
[IO.File]::Copy((Join-Path $PSScriptRoot 'DLSS5-Clean-Recovery.ps1'),$entry,$false)
$entryOutput=& (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File $entry -Action Plan
Check ($LASTEXITCODE -eq 0) 'default game directory failed through -File'
$entryPlan=($entryOutput -join "`n") | ConvertFrom-Json
Check ($entryPlan.game -eq $game) 'default game directory did not resolve to the script directory'
# The -File Restore entry point must list shared programs and exit 3 before any
# mutation; the exit-3 check runs ahead of Invoke-CleanRestore in the script.
# Get-CleanRegistrations reads the real registry, so a throwaway HKCU value
# points at the fixture layer whose ReShadeApps.ini names a real file under
# testRoot. If both shared-scope guards regressed, Invoke-CleanRestore would
# disable real ReShade registrations; a file lock cannot stop that, because any
# share mode that blocks the archive copy also blocks the child's own plan
# hashing of the same file. Passing a -Backup inside the game directory makes
# a regressed run refuse at the backup-outside-game check before any registry
# or file mutation. Opt-in: the default run must not touch the real registry.
$entryGame=Fixture 'entry-shared'
$entry=Join-Path $entryGame 'DLSS5-Clean-Recovery.ps1'
[IO.File]::Copy((Join-Path $PSScriptRoot 'DLSS5-Clean-Recovery.ps1'),$entry,$false)
$isAdmin=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(!$isAdmin -or $env:DLSS5_TEST_ELEVATED_ENTRY -ne '1') {
    Write-Output 'SKIP -File Restore shared-scope check (set DLSS5_TEST_ELEVATED_ENTRY=1 in an elevated shell; writes a temporary HKCU Vulkan layer value)'
} else {
    $entryLayer=$script:registry[0].name
    $entryOther=Join-Path $testRoot 'entry-shared-other\Game.exe'
    Put $entryOther 'fixture other game'
    Put (Join-Path (Split-Path $entryLayer -Parent) 'ReShadeApps.ini') ('Apps='+$entryGame+'\Endfield.exe,'+$entryOther)
    $regPath='SOFTWARE\Khronos\Vulkan\ImplicitLayers'
    $backupInsideGame=Join-Path $entryGame 'backup-inside-game'
    try {
        $regKey=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($regPath,$true)
        $regKey.SetValue($entryLayer,0,[Microsoft.Win32.RegistryValueKind]::DWord)
        $regKey.Dispose()
        $entryOutput=(& (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File $entry -Action Restore -Game $entryGame -Backup $backupInsideGame 2>&1 | Out-String)
        Check ($LASTEXITCODE -eq 3) "-File Restore exit code $LASTEXITCODE; output: $entryOutput"
        Check ($entryOutput -match [regex]::Escape($entryOther)) "shared program not listed; output: $entryOutput"
    } finally {
        $regKey=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($regPath,$true)
        if($regKey) { $regKey.DeleteValue($entryLayer,$false); $regKey.Dispose() }
    }
    foreach($leaf in @('Endfield.exe','dlss5-bridge.addon64','renodx-mfgunlock.addon64','ReShade.ini')) {
        Check (Test-Path -LiteralPath (Join-Path $entryGame $leaf)) '-File Restore mutated fixture files'
    }
    Check (!(Test-Path -LiteralPath $backupInsideGame)) '-File Restore created a backup despite the shared-scope stop'
}
Write-Output "PASS $script:checks clean-recovery checks. Mutations use mocked registry/process APIs; -File Plan is read-only and -File Restore must stop before mutation. Fixture retained: $testRoot"
