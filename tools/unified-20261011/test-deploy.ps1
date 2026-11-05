$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$test=Join-Path $root ('builds/unified-20261011/deploy-fixture-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $test '中文 游戏 & 路径'
$package=Join-Path $test 'package'
$evidence=Join-Path $test 'evidence'
[IO.Directory]::CreateDirectory($game) | Out-Null
[IO.Directory]::CreateDirectory($package) | Out-Null
function Put([string]$Path,[string]$Value) { [IO.File]::WriteAllText($Path,$Value,[Text.UTF8Encoding]::new($false)) }
function Digest([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Check([bool]$Value,[string]$Message) { if(!$Value) { throw $Message }; $script:checks++ }
$checks=0
foreach($name in @('Endfield.exe','sl.interposer.dll','ReShade.ini','dlss5-bridge.cfg','vk-present-adapter.ini','.deployment-test-fixture')) { Put (Join-Path $game $name) "original $name" }
Put (Join-Path $game 'dlss5-bridge.addon64') 'previous bridge'
$names=@('dlss5-bridge.addon64','renodx-dlss5.addon64','renodx-mfgunlock.addon64')
$entries=@()
foreach($name in $names) { Put (Join-Path $package $name) "candidate $name"; $entries+=@{path=$name;sha256=(Digest (Join-Path $package $name))} }
Put (Join-Path $package 'release-manifest.json') (@{version='fixture';files=$entries} | ConvertTo-Json -Depth 5)
Put (Join-Path $package 'SHA256SUMS.txt') 'fixture'
$before=@{}
Get-ChildItem -LiteralPath $game -File | ForEach-Object { $before[$_.Name]=Digest $_.FullName }
& "$PSScriptRoot/deploy.ps1" -Action Install -Game $game -Package $package -EvidenceRoot $evidence
$backup=(Get-ChildItem -LiteralPath $evidence -Directory | Sort-Object Name | Select-Object -Last 1).FullName
foreach($entry in $entries) { Check ((Digest (Join-Path $game $entry.path)) -eq $entry.sha256) 'Installed hash' }
& "$PSScriptRoot/deploy.ps1" -Action Rollback -Game $game -Package $package -Backup $backup
foreach($name in $before.Keys) { Check ((Digest (Join-Path $game $name)) -eq $before[$name]) 'Original did not restore' }
Check (!(Test-Path -LiteralPath (Join-Path $game 'renodx-dlss5.addon64'))) 'New file not moved out'
$failed=$false
try { & "$PSScriptRoot/deploy.ps1" -Action Install -Game $game -Package $package -EvidenceRoot $evidence -TestFailAfter 2 } catch { $failed=$_.Exception.Message -eq 'Injected deployment interruption' }
Check $failed 'Fault injection did not fire'
$backup=(Get-ChildItem -LiteralPath $evidence -Directory | Sort-Object Name | Select-Object -Last 1).FullName
& "$PSScriptRoot/deploy.ps1" -Action Rollback -Game $game -Package $package -Backup $backup
foreach($name in $before.Keys) { Check ((Digest (Join-Path $game $name)) -eq $before[$name]) 'Interrupted install restore failed' }
foreach($name in $names) { Put (Join-Path $game $name) "fixture $name" }
# A package update that changed the NR layout must retire the earlier payload
# path rather than leave a second, unread copy in the game folder. The fixture
# stands in for that older copy.
$superseded='dlss5\nr\common-310.8\nvngx_dlssnr.dll'
[IO.Directory]::CreateDirectory((Join-Path $game 'dlss5\nr\common-310.8')) | Out-Null
Put (Join-Path $game $superseded) 'older single-runtime layout'
$beforeRetire=Digest (Join-Path $game $superseded)
& "$PSScriptRoot/deploy.ps1" -Action Install -Game $game -Package $package -EvidenceRoot $evidence
Check (!(Test-Path -LiteralPath (Join-Path $game $superseded))) 'Superseded path not retired'
$retireBackup=(Get-ChildItem -LiteralPath $evidence -Directory | Sort-Object Name | Select-Object -Last 1).FullName
Check ((Digest (Join-Path $retireBackup 'superseded\dlss5\nr\common-310.8\nvngx_dlssnr.dll')) -eq $beforeRetire) 'Superseded copy not recorded'
& "$PSScriptRoot/deploy.ps1" -Action Rollback -Game $game -Package $package -Backup $retireBackup
Check ((Test-Path -LiteralPath (Join-Path $game $superseded)) -and ((Digest (Join-Path $game $superseded)) -eq $beforeRetire)) 'Superseded path not restored by rollback'
[IO.File]::Delete((Join-Path $game $superseded))
$recovery=Join-Path $game 'DLSS5-Recovery 故障恢复.cmd'
$launcher=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'DLSS5-Recovery.cmd'))
Check (@($launcher | Where-Object { $_ -gt 127 }).Count -eq 0) 'Recovery launcher is not ASCII-only'
Check (@([Text.Encoding]::ASCII.GetString($launcher) -split "`r`n" | Where-Object { $_ -notmatch '^\s*rem\b' -and $_ -match '^\s*:|\bgoto\s+:?\w' }).Count -eq 0) 'Recovery launcher uses labels or goto'
[IO.File]::WriteAllBytes($recovery,$launcher)
# A stand-in uninstaller records its arguments and archives the launcher while
# it runs, as the real one does, so the launcher must finish without rereading.
$stub=Join-Path $game 'DLSS5-Clean-Recovery.ps1'
$stubLog=Join-Path $test 'uninstall-args.txt'
[IO.File]::WriteAllText($stub,"param([string]`$Action,[string]`$Game)`r`n[IO.File]::WriteAllText('$($stubLog.Replace("'","''"))',`$Action+'|'+`$Game)`r`n[void](New-Item -ItemType Directory -Force -Path (Join-Path `$Game 'moved'))`r`nMove-Item -LiteralPath (Join-Path `$Game 'DLSS5-Recovery 故障恢复.cmd') -Destination (Join-Path `$Game 'moved')`r`nexit 5`r`n",[Text.UTF8Encoding]::new($true))
$empty=Join-Path $test 'empty-stdin.txt'; [IO.File]::WriteAllText($empty,'')
function Run-Launcher {
    $out=Join-Path $test 'launcher-out.txt'
    $p=Start-Process -FilePath $recovery -WorkingDirectory $test -WindowStyle Hidden -PassThru -RedirectStandardInput $empty -RedirectStandardOutput $out -RedirectStandardError ($out+'.err')
    $null=$p.Handle
    if(!$p.WaitForExit(60000)) { Stop-Process -Id $p.Id -Force; Check $false 'Recovery launcher hung' }
    return @{ code=$p.ExitCode; text=[IO.File]::ReadAllText($out)+[IO.File]::ReadAllText($out+'.err') }
}
$run=Run-Launcher
Check ($run.code -eq 5) "Recovery launcher did not return the uninstaller exit code ($($run.code))"
Check ([IO.File]::ReadAllText($stubLog) -eq ('Uninstall|'+$game+'\.')) 'Recovery launcher passed the wrong arguments'
Check (!(Test-Path -LiteralPath $recovery) -and $run.text -notmatch 'batch file') 'Recovery launcher failed after being archived'
[IO.File]::WriteAllBytes($recovery,$launcher); [IO.File]::Delete($stub)
$run=Run-Launcher
Check ($run.code -eq 1 -and $run.text -match 'missing') 'Recovery launcher ran without its uninstaller'
foreach($name in $names) { Check ([IO.File]::ReadAllText((Join-Path $game $name)) -eq "fixture $name") 'Recovery launcher touched add-on files' }
Write-Output "PASS $checks deployment/recovery checks; install, interrupted install, rollback, Chinese/space/ampersand path, uninstall launcher. Fixture retained: $test"
