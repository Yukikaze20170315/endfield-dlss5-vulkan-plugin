[CmdletBinding()]
param(
    [ValidateSet('Plan','Restore','Undo','Verify','Uninstall')][string]$Action='Plan',
    [string]$Game,
    [string]$Backup,
    [switch]$AllowSharedReShade,
    [string]$ExpectedUserSid
)
# Windows PowerShell 5.1 / PowerShell 7. No addon or ReShade DLL is loaded.
$ErrorActionPreference='Stop'
# Windows PowerShell 5.1 can evaluate parameter defaults before PSScriptRoot
# is populated when invoked with -File. Resolve the default in the script body.
if(!$Game) { $Game=$PSScriptRoot }
function Assert-CleanPath([string]$Path) {
    $p=[IO.Path]::GetFullPath($Path)
    while($p) {
        if((Test-Path -LiteralPath $p) -and ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked path refused: $p" }
        $p=Split-Path -Path $p -Parent
    }
}
function Clean-Hash([string]$Path) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $sha=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Assert-CleanClosed {
    if(@(Get-CimInstance Win32_Process -Filter "Name='Endfield.exe'").Count) { throw 'Exit Endfield before recovery / 请先退出游戏。' }
}
function Clean-Child([string]$Base,[string]$Relative) {
    if([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)|:') { throw 'Invalid relative path' }
    $p=[IO.Path]::GetFullPath((Join-Path $Base $Relative))
    if(!$p.StartsWith($Base.TrimEnd('\','/')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped root' }
    Assert-CleanPath $p
    return $p
}
function Save-CleanJson($Value,[string]$Path) {
    if(Test-Path -LiteralPath $Path) { throw "Existing evidence: $Path" }
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
}
function Get-CleanRegistrations {
    foreach($hive in @('LocalMachine','CurrentUser')) {
        foreach($view in @('Registry64','Registry32')) {
            $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::$hive,[Microsoft.Win32.RegistryView]::$view)
            try {
                foreach($subkey in @('SOFTWARE\Khronos\Vulkan\ImplicitLayers','SOFTWARE\Khronos\Vulkan\ExplicitLayers')) {
                    $key=$base.OpenSubKey($subkey,$false)
                    if(!$key) { continue }
                    try {
                        foreach($name in $key.GetValueNames()) {
                            $known=[IO.Path]::GetFileName($name) -match '^ReShade(?:32|64)?\.json$'
                            if(!$known -and (Test-Path -LiteralPath $name -PathType Leaf)) {
                                try { $json=Get-Content -LiteralPath $name -Raw | ConvertFrom-Json; $known=$json.layer.name -eq 'VK_LAYER_reshade' } catch { }
                            }
                            if(!$known) { continue }
                            if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::DWord) { throw "Unexpected registry type: $name" }
                            [pscustomobject]@{hive=$hive;view=$view;key=$subkey;name=$name;value=[int]$key.GetValue($name)}
                        }
                    } finally { $key.Dispose() }
                }
            } finally { $base.Dispose() }
        }
    }
}
function Set-CleanRegistration($Entry,[int]$Expected,[int]$Value) {
    $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::$($Entry.hive),[Microsoft.Win32.RegistryView]::$($Entry.view))
    try {
        $key=$base.OpenSubKey($Entry.key,$true)
        if(!$key) { throw 'Registry key disappeared' }
        try {
            if($key.GetValueKind($Entry.name) -ne [Microsoft.Win32.RegistryValueKind]::DWord) { throw 'Registry type changed since plan; retained' }
            $current=[int]$key.GetValue($Entry.name)
            if($current -ne $Expected -and $current -ne $Value) { throw 'Registry changed since plan; retained' }
            $key.SetValue($Entry.name,$Value,[Microsoft.Win32.RegistryValueKind]::DWord)
            if([int]$key.GetValue($Entry.name) -ne $Value) { throw 'Registry verification failed' }
        } finally { $key.Dispose() }
    } finally { $base.Dispose() }
}
function Get-CleanFiles([string]$Game) {
    $names=@('dlss5','reshade-shaders','dlss5-bridge.addon64','renodx-dlss5.addon64','renodx-mfgunlock.addon64',
      'dlss5-bridge.addon64.off','renodx-dlss5.addon64.off','renodx-mfgunlock.addon64.off',
      'dlss5-bridge.cfg','vk-present-adapter.ini','ReShade.ini','ReShadePreset.ini','DLSS 5.ini',
      'payload-manifest.json','release-manifest.json','SHA256SUMS.txt',
      'dlss5-bridge.log','dlss5-observe.log','dlss5-observe.prev.log','dlss5-sl.log',
      'RenoDX-DLSS5-crash.log','ReShade.log','ReShade.log1','ReShade.log2',
      'RenoDX-DLSS5-normtrace.csv','RenoDX-DLSS5-edittrace.csv','DLSS5-上次启动异常-说明.txt')
    foreach($leaf in @('dxgi.dll','d3d9.dll','d3d10.dll','d3d11.dll','opengl32.dll','vulkan-1.dll')) {
        $p=Clean-Child $Game $leaf
        if((Test-Path -LiteralPath $p -PathType Leaf) -and ([Diagnostics.FileVersionInfo]::GetVersionInfo($p).ProductName -match 'ReShade')) { $names+=$leaf }
    }
    # A bare nvngx_dlssnr.dll beside the executable belongs to the pack only when
    # it is one of the two runtimes this project ships; the archived copies live
    # under dlss5\, which is already listed above and removed wholesale.
    $nr=Clean-Child $Game 'nvngx_dlssnr.dll'
    $nr_hashes=@('e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e',
                 '6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927')
    if((Test-Path -LiteralPath $nr -PathType Leaf) -and ($nr_hashes -contains (Clean-Hash $nr))) { $names+='nvngx_dlssnr.dll' }
    $queue=[Collections.Generic.Queue[string]]::new()
    foreach($name in $names) { if(Test-Path -LiteralPath (Clean-Child $Game $name)) { $queue.Enqueue($name) } }
    while($queue.Count) {
        $relative=$queue.Dequeue(); $path=Clean-Child $Game $relative
        $item=Get-Item -LiteralPath $path -Force
        if($item.PSIsContainer) {
            foreach($child in [IO.Directory]::GetFileSystemEntries($path)) { $queue.Enqueue($child.Substring($Game.Length+1)) }
        } else { [pscustomobject]@{path=$relative;hash=(Clean-Hash $path)} }
    }
}
function Get-CleanSharedApps($Registrations,[string]$Game) {
    $shared=[Collections.Generic.List[string]]::new()
    $stale=[Collections.Generic.List[string]]::new()
    $current=[IO.Path]::GetFullPath((Join-Path $Game 'Endfield.exe'))
    $seen=@{}
    foreach($entry in $Registrations) {
        $ini=Join-Path (Split-Path -Path $entry.name -Parent) 'ReShadeApps.ini'
        if($seen.ContainsKey($ini)) { continue }; $seen[$ini]=$true
        if(!(Test-Path -LiteralPath $ini)) { $shared.Add('Unknown application scope / 未找到应用绑定清单'); continue }
        Assert-CleanPath $ini
        $text=[IO.File]::ReadAllText($ini)
        $match=[regex]::Match($text,'(?im)^Apps=(.*)$')
        if(!$match.Success -or !$match.Groups[1].Value.Trim()) { $shared.Add('Unknown application scope / 未知应用范围'); continue }
        foreach($app in $match.Groups[1].Value.Split(',')) {
            $app=$app.Trim().Trim('"').Trim()
            if(!$app) { continue }
            try { $p=[IO.Path]::GetFullPath($app) } catch { $stale.Add($app); continue }
            if([string]::Equals($p,$current,[StringComparison]::OrdinalIgnoreCase)) { continue }
            if(Test-Path -LiteralPath $p -PathType Leaf) { $shared.Add($app) } else { $stale.Add($app) }
        }
    }
    return [pscustomobject]@{shared=$shared;stale=$stale}
}
# The uninstaller's own files. They are archived only after everything else
# succeeded, so a failed driver or VRAM budget restore can be retried in place.
function Move-CleanTools([string]$Game,[string]$Backup) {
    $moved=[Collections.Generic.List[string]]::new()
    $left=[Collections.Generic.List[string]]::new()
    foreach($name in @('DLSS5-Recovery 故障恢复.cmd','DLSS5-Recovery.cmd','DLSS5-Clean-Recovery.ps1','DLSS5-RestoreDriver.exe',
                       'DLSS5-使用与恢复 Usage and Recovery.md','DLSS5-Clean-Recovery-result.txt')) {
        $source=Clean-Child $Game $name
        if(!(Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        try {
            $target=Clean-Child (Join-Path $Backup 'tools') $name
            [IO.Directory]::CreateDirectory((Split-Path -Path $target -Parent)) | Out-Null
            [IO.File]::Move($source,$target)
            $moved.Add($name)
        } catch { $left.Add($name) }
    }
    return [pscustomobject]@{moved=@($moved);left=@($left)}
}
# DLSS5-RestoreDriver.exe uses game\dlss5\state as its first-choice state folder:
# it recreates the folder and may write records the plan never listed. Those
# files go to <backup>\leftover, then the emptied pack folders are removed.
# Reparse points are neither followed nor removed.
function Move-CleanLeftovers([string]$Game,[string]$Backup) {
    $moved=[Collections.Generic.List[string]]::new()
    $left=[Collections.Generic.List[string]]::new()
    $reparse=[IO.FileAttributes]::ReparsePoint
    foreach($root in @('dlss5','reshade-shaders')) {
        $top=Clean-Child $Game $root
        if(!(Test-Path -LiteralPath $top -PathType Container)) { continue }
        if(([IO.File]::GetAttributes($top) -band $reparse)) { $left.Add($root); continue }
        $folders=[Collections.Generic.List[string]]::new()
        $queue=[Collections.Generic.Queue[string]]::new(); $queue.Enqueue($top)
        while($queue.Count) {
            $folder=$queue.Dequeue(); $folders.Add($folder)
            foreach($child in [IO.Directory]::GetFileSystemEntries($folder)) {
                $relative=$child.Substring($Game.Length+1)
                $attributes=[IO.File]::GetAttributes($child)
                if($attributes -band $reparse) { $left.Add($relative); continue }
                if($attributes -band [IO.FileAttributes]::Directory) { $queue.Enqueue($child); continue }
                try {
                    $target=Clean-Child (Join-Path $Backup 'leftover') $relative
                    [IO.Directory]::CreateDirectory((Split-Path -Path $target -Parent)) | Out-Null
                    [IO.File]::Move($child,$target)
                    $moved.Add($relative)
                } catch { $left.Add($relative) }
            }
        }
        # Deepest first; a folder that still holds anything stays.
        for($i=$folders.Count-1; $i -ge 0; $i--) {
            try { if(![IO.Directory]::EnumerateFileSystemEntries($folders[$i]).GetEnumerator().MoveNext()) { [IO.Directory]::Delete($folders[$i],$false) } } catch {}
        }
    }
    return [pscustomobject]@{moved=@($moved);left=@($left)}
}
function Invoke-CleanDriver([string]$Game) {
    $helper=Join-Path $PSScriptRoot 'DLSS5-RestoreDriver.exe'
    Assert-CleanPath $helper
    if(!(Test-Path -LiteralPath $helper -PathType Leaf)) { return 4 }
    # Write-Host goes to the Information stream, so the helper's messages reach the elevated '*>' report.
    & $helper $Game | ForEach-Object { Write-Host $_ }
    return $LASTEXITCODE
}
function Get-CleanPlan([string]$Game) {
    $Game=[IO.Path]::GetFullPath($Game).TrimEnd('\','/')
    Assert-CleanPath $Game
    if(!(Test-Path -LiteralPath (Join-Path $Game 'Endfield.exe') -PathType Leaf)) { throw 'Not an Endfield directory' }
    $registrations=@(Get-CleanRegistrations)
    $scope=Get-CleanSharedApps $registrations $Game
    [pscustomobject]@{schema=1;game=$Game;utc=[DateTime]::UtcNow.ToString('o');registrations=$registrations;
      shared_apps=@($scope.shared);stale_apps=@($scope.stale);files=@(Get-CleanFiles $Game)}
}
function Invoke-CleanRestore($Plan,[string]$Backup,[bool]$AllowShared) {
    Assert-CleanClosed
    if($Plan.shared_apps.Count -and !$AllowShared) { throw 'ReShade may serve other Vulkan games. Review shared_apps and explicitly allow shared ReShade removal.' }
    $Backup=[IO.Path]::GetFullPath($Backup).TrimEnd('\','/')
    Assert-CleanPath $Backup
    if($Backup -eq $Plan.game -or $Backup.StartsWith($Plan.game+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Backup must be outside the game directory' }
    if(Test-Path -LiteralPath $Backup) { throw 'Backup destination already exists' }
    # Revalidate every source before any registry or game mutation.
    foreach($entry in $Plan.files) { if((Clean-Hash (Clean-Child $Plan.game $entry.path)) -ne $entry.hash) { throw 'Source changed after plan' } }
    [IO.Directory]::CreateDirectory($Backup) | Out-Null
    Save-CleanJson $Plan (Join-Path $Backup 'recovery.json')
    foreach($entry in $Plan.files) {
        $target=Clean-Child (Join-Path $Backup 'files') $entry.path
        [IO.Directory]::CreateDirectory((Split-Path -Path $target -Parent)) | Out-Null
        [IO.File]::Copy((Clean-Child $Plan.game $entry.path),$target,$false)
        if((Clean-Hash $target) -ne $entry.hash) { throw 'Backup hash mismatch; sources retained' }
    }
    Assert-CleanClosed
    foreach($entry in $Plan.registrations) { if($entry.value -eq 0) { Set-CleanRegistration $entry 0 1 } }
    # Driver failure never blocks removal of broken render components.
    try { $driver=Invoke-CleanDriver $Plan.game } catch { $driver=4; Write-Warning $_ }
    # Records the helper itself rewrites (driver journals, VRAM budget change time).
    $helperWritten='^dlss5[\\/]state[\\/](drs-(journal|fg)|vram-budget-change)\.state$'
    $afterDriver=@{}
    foreach($entry in $Plan.files) {
        if($entry.path -match $helperWritten) { $afterDriver[$entry.path]=Clean-Hash (Clean-Child $Plan.game $entry.path) }
    }
    Save-CleanJson $afterDriver (Join-Path $Backup 'post-driver-hashes.json')
    # Preserve post-recovery records too: undo must not resurrect a pending DRS journal.
    foreach($entry in $Plan.files) {
        Assert-CleanClosed
        $source=Clean-Child $Plan.game $entry.path
        if(!(Test-Path -LiteralPath $source)) { throw 'Source disappeared during recovery' }
        $current=Clean-Hash $source
        if($current -ne $entry.hash -and $entry.path -notmatch $helperWritten) { throw 'Source changed during recovery; retained' }
        $moved=Clean-Child (Join-Path $Backup 'removed') $entry.path
        [IO.Directory]::CreateDirectory((Split-Path -Path $moved -Parent)) | Out-Null
        [IO.File]::Move($source,$moved)
        if((Clean-Hash $moved) -ne $current) { throw 'Archive verification failed' }
    }
    $remaining=@(Get-CleanRegistrations | Where-Object value -eq 0)
    if($remaining.Count) { throw 'An enabled ReShade Vulkan registration remains' }
    $tools=[pscustomobject]@{moved=@();left=@()}
    $leftovers=[pscustomobject]@{moved=@();left=@()}
    # After a partial restore the helper's records stay for the retry, which archives them.
    if($driver -eq 0) { $leftovers=Move-CleanLeftovers $Plan.game $Backup; $tools=Move-CleanTools $Plan.game $Backup }
    $script:CleanRestoreDriverExit=$driver
    Save-CleanJson @{status='render-components-removed; launcher-repair-required';driver_exit=$driver;game=$Plan.game;backup=$Backup;
      tools_archived=$tools.moved;tools_left=$tools.left;leftovers_archived=$leftovers.moved;leftovers_left=$leftovers.left;
      note='Run launcher file verification/repair. Game root SL/NGX files may have been changed by older tools; they were not replaced with stale backups.'} (Join-Path $Backup 'result.json')
    Write-Output "ReShade Vulkan loading disabled; NR components archived. Backup: $Backup"
    Write-Output "请在启动器执行校验/修复，恢复当前游戏版本的原装文件。 / Run launcher verification/repair."
    # Helper exit codes: 6 VRAM budget only, 8 VRAM budget and driver, others driver or helper failure.
    if($driver -eq 6 -or $driver -eq 8) { Write-Warning '显存预算仍是 64/64，未能还原。卸载工具已保留，请关闭其他程序后再运行一次 DLSS5-Recovery 故障恢复.cmd。 / The Windows VRAM budget is still 64/64. The uninstaller was kept; close other programs and run DLSS5-Recovery 故障恢复.cmd again.' }
    if($driver -ne 0 -and $driver -ne 6) { Write-Warning '驱动改动未能全部还原，记录已保留。卸载工具已保留，可再运行一次 DLSS5-Recovery 故障恢复.cmd 重试。 / Driver recovery is incomplete; the journals and the uninstaller were kept. Run DLSS5-Recovery 故障恢复.cmd again to retry. See result.json.' }
    if($tools.left.Count) { Write-Output ('以下卸载工具文件未能移走，可手动删除： / These uninstaller files could not be moved and can be deleted by hand: '+($tools.left -join ', ')) }
    if($leftovers.left.Count) { Write-Output ('以下文件未能移走，可手动删除： / These files could not be moved and can be deleted by hand: '+($leftovers.left -join ', ')) }
}
function Invoke-CleanUndo([string]$Backup,[string]$ExpectedGame) {
    $Backup=[IO.Path]::GetFullPath($Backup); Assert-CleanPath $Backup; Assert-CleanClosed
    $plan=Get-Content -LiteralPath (Join-Path $Backup 'recovery.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($plan.schema -ne 1) { throw 'Unsupported recovery record' }
    if(!$ExpectedGame -or ![string]::Equals([IO.Path]::GetFullPath($ExpectedGame).TrimEnd('\','/'),[IO.Path]::GetFullPath($plan.game).TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Backup belongs to a different game directory' }
    Assert-CleanPath $plan.game
    $afterDriver=@{}
    $hashFile=Join-Path $Backup 'post-driver-hashes.json'
    if(Test-Path -LiteralPath $hashFile) {
        $hashObject=Get-Content -LiteralPath $hashFile -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach($property in $hashObject.PSObject.Properties) { $afterDriver[$property.Name]=[string]$property.Value }
    }
    $currentRegistrations=@(Get-CleanRegistrations)
    foreach($entry in $plan.registrations) {
        $match=@($currentRegistrations | Where-Object { $_.hive -eq $entry.hive -and $_.view -eq $entry.view -and $_.key -eq $entry.key -and $_.name -eq $entry.name })
        if($match.Count -ne 1 -or ($match[0].value -ne $entry.value -and $match[0].value -ne 1)) { throw 'Registry changed externally; refusing undo' }
    }
    foreach($entry in $plan.files) {
        $source=Clean-Child (Join-Path $Backup 'removed') $entry.path
        $expected=$entry.hash
        if(Test-Path -LiteralPath $source) { if($afterDriver.ContainsKey($entry.path)) { $expected=$afterDriver[$entry.path] } }
        else { $source=Clean-Child (Join-Path $Backup 'files') $entry.path }
        if((Clean-Hash $source) -ne $expected) { throw 'Archive modified; refusing undo' }
        $target=Clean-Child $plan.game $entry.path
        if((Test-Path -LiteralPath $target) -and (Clean-Hash $target) -ne (Clean-Hash $source)) { throw "Destination changed; retained: $target" }
    }
    foreach($entry in $plan.files) {
        $source=Clean-Child (Join-Path $Backup 'removed') $entry.path
        if(!(Test-Path -LiteralPath $source)) { $source=Clean-Child (Join-Path $Backup 'files') $entry.path }
        $target=Clean-Child $plan.game $entry.path
        if(!(Test-Path -LiteralPath $target)) { [IO.Directory]::CreateDirectory((Split-Path -Path $target -Parent)) | Out-Null; [IO.File]::Copy($source,$target,$false) }
        if((Clean-Hash $target) -ne (Clean-Hash $source)) { throw 'Undo copy verification failed' }
    }
    foreach($entry in $plan.registrations) {
        $match=@($currentRegistrations | Where-Object { $_.hive -eq $entry.hive -and $_.view -eq $entry.view -and $_.key -eq $entry.key -and $_.name -eq $entry.name })
        if($entry.value -eq 0 -and $match[0].value -eq 1) { Set-CleanRegistration $entry 1 0 }
    }
    Write-Output 'Archived render components restored. Driver overrides are not reapplied.'
}
function Read-CleanYes([string]$Prompt) {
    $answer=Read-Host $Prompt
    return [bool]($answer -and $answer.Trim() -match '^[Yy]')
}
function Wait-CleanClose { Read-Host '按回车键关闭 / Press Enter to close' | Out-Null }
# Runs Restore in a child sharing this console, so no output is re-encoded and
# the child elevates itself exactly as a direct -Action Restore call does.
function Invoke-CleanChild([string]$Game,[bool]$AllowShared) {
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Action Restore -Game "'+$Game+'"'
    if($AllowShared) { $arguments+=' -AllowSharedReShade' }
    $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -NoNewWindow -PassThru
    $null=$child.Handle
    $child.WaitForExit()
    return [int]$child.ExitCode
}
function Invoke-CleanUninstall([string]$Game) {
    # A trailing backslash would escape the closing quote of the child argument.
    $Game=[IO.Path]::GetFullPath($Game)
    if($Game.EndsWith('\')) { $Game+='.' }
    Write-Host ''
    Write-Host 'DLSS5 NR 完全卸载 / Full uninstall'
    Write-Host '将停用 ReShade 的 Vulkan 加载，撤除三个插件、NR 运行文件以及本套件的配置和日志，并还原本套件改过的驱动设置和 Windows 显存预算设置（显存预算重启电脑后生效）。'
    Write-Host '撤除的文件移到游戏目录旁的 DLSS5-Recovery-Backups 文件夹，不直接删除；卸载工具自己最后也会移进去。'
    Write-Host 'This disables ReShade Vulkan loading, archives the three add-ons, the NR runtime files and this package''s settings and logs, and restores its driver and Windows VRAM budget changes (restart Windows for the VRAM budget). Files are moved to DLSS5-Recovery-Backups next to the game folder; the uninstaller itself is moved there last.'
    Write-Host ('游戏目录 / Game folder: '+$Game)
    try { Assert-CleanClosed } catch {
        Write-Host '请先完全退出游戏，再运行本工具。未做任何改动。 / Exit Endfield first; nothing was changed.'
        Wait-CleanClose; return 3
    }
    if(!(Read-CleanYes '确认卸载？输入 Y 后按回车继续，直接回车取消 / Type Y and press Enter to uninstall')) {
        Write-Host '已取消，未做任何改动。 / Cancelled; nothing was changed.'
        Wait-CleanClose; return 0
    }
    $rc=Invoke-CleanChild $Game $false
    if($rc -eq 3) {
        if(!(Read-CleanYes '仍要继续卸载？输入 Y 后按回车继续，直接回车取消 / Type Y and press Enter to continue anyway')) {
            Write-Host '已取消，未做任何改动。 / Cancelled; nothing was changed.'
            Wait-CleanClose; return 0
        }
        $rc=Invoke-CleanChild $Game $true
    }
    Write-Host ''
    if($rc -eq 0) {
        Write-Host '卸载完成。请在游戏启动器里对游戏执行一次“校验/修复”。确认游戏正常后，可以手动删除 DLSS5-Recovery-Backups 文件夹。'
        Write-Host 'Uninstall finished. Run file verification/repair in the game launcher. Once the game works, you may delete DLSS5-Recovery-Backups by hand.'
    } elseif($rc -eq 7) {
        Write-Host '没有取得管理员权限，未做任何改动。 / Administrator permission was not granted; nothing was changed.'
    } elseif($rc -eq 9) {
        Write-Host '插件和 NR 文件已撤除，但驱动设置或 Windows 显存预算没有完全还原（见上方提示）。卸载工具已保留：请关闭其他程序（尤其是 NVIDIA App、NVIDIA 控制面板）后，再双击运行一次。'
        Write-Host 'The add-ons and NR files were removed, but the driver settings or the Windows VRAM budget were not fully restored (see above). The uninstaller was kept: close other programs (especially NVIDIA App and NVIDIA Control Panel) and run it again.'
    } else {
        Write-Host '卸载未完成，已撤除和未撤除的文件都保留着。请把本窗口的信息截图发给开发者。 / Uninstall did not finish; all files are kept. Send a screenshot of this window to the developer.'
    }
    Wait-CleanClose
    return $rc
}
if($MyInvocation.InvocationName -eq '.') { return }
try {
    if($Action -eq 'Uninstall') { exit (Invoke-CleanUninstall $Game) }
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if($ExpectedUserSid -and $ExpectedUserSid -ne $sid) { throw 'UAC account changed; use the original Windows account.' }
    if($Action -in @('Restore','Undo')) {
        Assert-CleanClosed
        $principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
        if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            # EncodedCommand avoids interpreting quotes/metacharacters in filesystem paths.
            $q={param($s) "'"+$s.Replace("'","''")+"'"}
            $command='& '+(& $q $PSCommandPath)+' -Action '+$Action+' -Game '+(& $q $Game)+' -ExpectedUserSid '+(& $q $sid)
            if($Backup) { $command+=' -Backup '+(& $q $Backup) }
            if($AllowSharedReShade) { $command+=' -AllowSharedReShade' }
            $report=Join-Path ([IO.Path]::GetTempPath()) ('DLSS5-Clean-Recovery-result-'+[guid]::NewGuid().ToString('N')+'.txt')
            $command+=' *> '+(& $q $report)+'; exit $LASTEXITCODE'
            $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
            try {
                $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -WindowStyle Hidden -PassThru -Wait -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded)
            } catch {
                Write-Output '没有取得管理员权限，未做任何改动。 / Administrator permission was not granted; nothing was changed.'
                exit 7
            }
            if(Test-Path -LiteralPath $report) {
                Get-Content -LiteralPath $report | Out-Host
                if($child.ExitCode -eq 0) { Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue }
                else { Write-Output ('完整记录 / Full report: '+$report) }
            }
            exit $child.ExitCode
        }
    }
    if($Action -eq 'Undo') { if(!$Backup) { throw 'Undo requires -Backup' }; Invoke-CleanUndo $Backup $Game; exit 0 }
    $plan=Get-CleanPlan $Game
    if($Action -eq 'Plan') { $plan | ConvertTo-Json -Depth 10; exit 0 }
    if($Action -eq 'Verify') {
        @{enabled_reshade=@($plan.registrations | Where-Object value -eq 0).Count;remaining_component_files=$plan.files.Count;
          original_game_files='Launcher verification/repair required; not inferred from component removal'} | ConvertTo-Json
        exit 0
    }
    if($plan.stale_apps.Count) {
        Write-Output '已忽略不存在的旧程序路径 / Ignored program paths that no longer exist:'
        foreach($app in $plan.stale_apps) { Write-Output ('  '+$app) }
    }
    if($plan.shared_apps.Count -and !$AllowSharedReShade.IsPresent) {
        Write-Output '以下程序也在使用同一个 ReShade（Vulkan）。撤除会让它们也暂时不再加载 ReShade： / These programs share the same Vulkan ReShade. Removal will also stop ReShade from loading for them:'
        foreach($app in $plan.shared_apps) { Write-Output ('  '+$app) }
        exit 3
    }
    if(!$Backup) { $Backup=Join-Path (Split-Path -Path $plan.game -Parent) ('DLSS5-Recovery-Backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)) }
    Invoke-CleanRestore $plan $Backup $AllowSharedReShade.IsPresent
    # 9: components archived, but the driver or VRAM budget restore is incomplete.
    if($script:CleanRestoreDriverExit) { exit 9 }
    exit 0
} catch { Write-Error -ErrorAction Continue $_; exit 1 }
