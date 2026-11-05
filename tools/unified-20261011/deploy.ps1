[CmdletBinding()]
param(
    [ValidateSet('Install','Rollback','Verify')][string]$Action='Verify',
    [string]$Game='C:\Program Files\Hypergryph Launcher\games\Arknights Endfield',
    [string]$Package,
    [string]$Backup,
    [string]$EvidenceRoot,
    [int]$TestFailAfter=0
)
# Windows PowerShell 5.1 / PowerShell 7. This script never writes user settings,
# driver profiles or original game DLLs. Rollback preserves newly installed files.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$Package) { $Package=Join-Path $root 'dist/unified-20261015q-candidate' }
if(!$EvidenceRoot) { $EvidenceRoot=Join-Path $root 'evidence/unified-20261011' }
function Assert-Plain([string]$Path) {
    $walk=[IO.Path]::GetFullPath($Path)
    while($walk) {
        if(Test-Path -LiteralPath $walk) {
            if((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse point refused: $walk"
            }
        }
        $walk=Split-Path -Path $walk -Parent
    }
}
function Child([string]$Base,[string]$Relative) {
    if([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)|:') { throw 'Invalid relative path' }
    $path=[IO.Path]::GetFullPath((Join-Path $Base $Relative))
    if(!$path.StartsWith($Base.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped root' }
    Assert-Plain $path
    return $path
}
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Closed {
    if(@(Get-CimInstance Win32_Process -Filter "Name='Endfield.exe'").Count) { throw 'Exit Endfield before deployment or rollback' }
}
function Copy-Verified([string]$Source,[string]$Target,[string]$Expected) {
    Assert-Plain $Source
    Assert-Plain $Target
    if(Test-Path -LiteralPath $Target) { throw "Refusing existing copy destination: $Target" }
    [IO.Directory]::CreateDirectory((Split-Path $Target -Parent)) | Out-Null
    [IO.File]::Copy($Source,$Target,$false)
    if((Hash $Target) -ne $Expected) { throw "Copy verification failed: $Target" }
}
function Replace-Verified([string]$Source,[string]$Target,[string]$Expected) {
    $staged=$Target+'.install-'+[guid]::NewGuid().ToString('N')
    Copy-Verified $Source $staged $Expected
    Closed
    if(Test-Path -LiteralPath $Target) { [IO.File]::Replace($staged,$Target,[NullString]::Value) }
    else { [IO.File]::Move($staged,$Target) }
    if((Hash $Target) -ne $Expected) { throw "Installed hash mismatch: $Target" }
}
function Save-NewJson($Value,[string]$Path) {
    if(Test-Path -LiteralPath $Path) { throw "Existing evidence record: $Path" }
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
}
$Game=[IO.Path]::GetFullPath($Game).TrimEnd('\','/')
$Package=[IO.Path]::GetFullPath($Package).TrimEnd('\','/')
$EvidenceRoot=[IO.Path]::GetFullPath($EvidenceRoot).TrimEnd('\','/')
foreach($path in @($Game,$Package,$EvidenceRoot)) { Assert-Plain $path }
if(!(Test-Path -LiteralPath (Join-Path $Game 'Endfield.exe')) -or !(Test-Path -LiteralPath (Join-Path $Game 'sl.interposer.dll'))) { throw 'Not an Endfield directory' }
Closed

if($Action -eq 'Rollback') {
    if(!$Backup) { throw 'Rollback requires -Backup with a recorded deployment directory' }
    $Backup=[IO.Path]::GetFullPath($Backup)
    Assert-Plain $Backup
    $record=Get-Content -LiteralPath (Join-Path $Backup 'deployment.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($record.game -ne $Game) { throw 'Backup belongs to a different game directory' }
    # Validate the complete restore before mutating any destination.
    foreach($entry in $record.files) {
        $target=Child $Game $entry.path
        if($entry.existed) {
            $original=Child (Join-Path $Backup 'original') $entry.path
            if((Hash $original) -ne $entry.before) { throw 'Backup verification failed' }
        }
        if(Test-Path -LiteralPath $target) {
            $current=Hash $target
            if($current -ne $entry.after -and $current -ne $entry.before) { throw "Changed since deployment; retained: $target" }
        }
    }
    # Validate the superseded-file restore as well, before anything moves. A
    # deployment that retired an earlier package's payload recorded the copy;
    # rollback puts it back so the older package's layout is whole again.
    foreach($entry in @($record.retire)) {
        if(!$entry) { continue }
        $copy=Child (Join-Path $Backup 'superseded') $entry.path
        if((Hash $copy) -ne $entry.before) { throw 'Superseded backup verification failed' }
        $target=Child $Game $entry.path
        if((Test-Path -LiteralPath $target) -and (Hash $target) -ne $entry.before) { throw "Superseded path is occupied; retained: $target" }
    }
    $retained=Join-Path $Backup ('rollback-retained-'+[guid]::NewGuid().ToString('N'))
    foreach($entry in @($record.retire)) {
        if(!$entry) { continue }
        $target=Child $Game $entry.path
        if(Test-Path -LiteralPath $target) { continue }
        Assert-Plain $target
        [IO.Directory]::CreateDirectory((Split-Path $target -Parent)) | Out-Null
        Copy-Verified (Child (Join-Path $Backup 'superseded') $entry.path) $target $entry.before
    }
    foreach($entry in $record.files) {
        $target=Child $Game $entry.path
        if((Test-Path -LiteralPath $target) -and (Hash $target) -eq $entry.before) { continue }
        if(Test-Path -LiteralPath $target) {
            Copy-Verified $target (Child $retained $entry.path) $entry.after
        }
        if($entry.existed) { Replace-Verified (Child (Join-Path $Backup 'original') $entry.path) $target $entry.before }
        elseif(Test-Path -LiteralPath $target) {
            # Preserve the new file; do not delete it during rollback.
            $moved=Child ($retained+'-moved') $entry.path
            [IO.Directory]::CreateDirectory((Split-Path $moved -Parent)) | Out-Null
            Closed
            [IO.File]::Move($target,$moved)
        }
    }
    Save-NewJson @{action='Rollback';game=$Game;retained=$retained;utc=[DateTime]::UtcNow.ToString('o')} (Join-Path $Backup ('rollback-'+[guid]::NewGuid().ToString('N')+'.json'))
    Write-Output "Rollback verified. User configurations and runtime state preserved. Backup: $Backup"
    return
}

$manifest=Get-Content -LiteralPath (Join-Path $Package 'release-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$files=@($manifest.files)+@(
    @{path='release-manifest.json';sha256=(Hash (Join-Path $Package 'release-manifest.json'))},
    @{path='SHA256SUMS.txt';sha256=(Hash (Join-Path $Package 'SHA256SUMS.txt'))}
)
$protected=@('ReShade.ini','dlss5-bridge.cfg','vk-present-adapter.ini')
$protected+=@(Get-ChildItem -LiteralPath $Game -File | Where-Object { $_.Name -like 'sl.*.dll' -or $_.Name -like 'nvngx*.dll' } | ForEach-Object Name)
$snapshot=@()
foreach($name in $protected) {
    $path=Child $Game $name
    if(Test-Path -LiteralPath $path) { $snapshot+=@{path=$name;hash=(Hash $path)} }
}
$plan=@()
foreach($entry in $files) {
    $relative=[string]$entry.path
    if($relative -notmatch '^(dlss5/|dlss5-bridge\.addon64$|renodx-dlss5\.addon64$|renodx-mfgunlock\.addon64$|DLSS5-|payload-manifest\.json$|release-manifest\.json$|SHA256SUMS\.txt$)') { throw "Unexpected payload: $relative" }
    $source=Child $Package $relative
    $target=Child $Game $relative
    if((Hash $source) -ne $entry.sha256) { throw "Package checksum mismatch: $relative" }
    $exists=Test-Path -LiteralPath $target
    $before=if($exists) { Hash $target } else { '' }
    if($before -eq $entry.sha256) { continue }
    $plan+=@{path=$relative;existed=$exists;before=$before;after=[string]$entry.sha256}
}
# Superseded payload paths. An earlier package kept one NR runtime at a single
# path; this one ships the runtimes under per-generation directories. Nothing
# reads the old copy, but leaving ~166 MB in the game folder would contradict
# the small, simple file layout this package is for. It is moved into the
# backup rather than deleted, and rollback moves it back, so retiring it is
# reversible and the user's disk is not the only record.
$superseded=@('dlss5/nr/common-310.8/nvngx_dlssnr.dll')
$retire=@()
foreach($relative in $superseded) {
    if($files.path -contains $relative) { throw "Superseded path is still a payload: $relative" }
    $target=Child $Game $relative
    if(Test-Path -LiteralPath $target -PathType Leaf) {
        $retire+=@{path=$relative;before=(Hash $target)}
    }
}
if($Action -eq 'Verify') {
    @{package=$manifest.version;changed_files=$plan.Count;files=$plan;retire=$retire;preserved=$snapshot} | ConvertTo-Json -Depth 5
    return
}
if($TestFailAfter -and !(Test-Path -LiteralPath (Child $Game '.deployment-test-fixture'))) { throw 'Fault injection is restricted to a marked fixture' }
if(!$plan.Count -and !$retire.Count) { Write-Output 'Package already matches; no changes made.'; return }
$Backup=Join-Path $EvidenceRoot ('install-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
[IO.Directory]::CreateDirectory($Backup) | Out-Null
foreach($entry in $plan) {
    if($entry.existed) { Copy-Verified (Child $Game $entry.path) (Child (Join-Path $Backup 'original') $entry.path) $entry.before }
}
foreach($entry in $retire) {
    Copy-Verified (Child $Game $entry.path) (Child (Join-Path $Backup 'superseded') $entry.path) $entry.before
}
foreach($entry in $snapshot) {
    if($entry.path -in @('ReShade.ini','dlss5-bridge.cfg','vk-present-adapter.ini')) {
        Copy-Verified (Child $Game $entry.path) (Child (Join-Path $Backup 'config-snapshot') $entry.path) $entry.hash
    }
}
foreach($log in @(Get-ChildItem -LiteralPath $Game -File | Where-Object { $_.Name -like '*.log' })) {
    Copy-Verified $log.FullName (Child (Join-Path $Backup 'logs') $log.Name) (Hash $log.FullName)
}
Save-NewJson @{game=$Game;package=$Package;version=$manifest.version;utc=[DateTime]::UtcNow.ToString('o');files=$plan;retire=$retire;preserved=$snapshot} (Join-Path $Backup 'deployment.json')
Write-Output "Verified backup: $Backup"
$count=0
try {
    foreach($entry in $plan) {
        $target=Child $Game $entry.path
        if($entry.existed -and (Hash $target) -ne $entry.before) { throw 'Destination changed after backup' }
        if(!$entry.existed -and (Test-Path -LiteralPath $target)) { throw 'Destination appeared after backup' }
        Replace-Verified (Child $Package $entry.path) $target $entry.after
        ++$count
        if($TestFailAfter -and $count -eq $TestFailAfter) { throw 'Injected deployment interruption' }
    }
    foreach($entry in $snapshot) { if((Hash (Child $Game $entry.path)) -ne $entry.hash) { throw "Protected file changed: $($entry.path)" } }
    $retired=0
    foreach($entry in $retire) {
        $target=Child $Game $entry.path
        # The copy in superseded/ is the record; the destination must still be
        # what was recorded, or the user changed it and it is left alone.
        if((Hash $target) -ne $entry.before) { throw "Superseded file changed after backup: $($entry.path)" }
        Closed
        [IO.File]::Delete($target)
        ++$retired
    }
    Save-NewJson @{result='installed-and-verified';count=$count;retired=$retired;utc=[DateTime]::UtcNow.ToString('o')} (Join-Path $Backup 'success.json')
    Write-Output "Installed and verified $count files; retired $retired superseded file(s). All $($snapshot.Count) protected hashes unchanged."
} catch {
    Write-Warning "Installation stopped; backup and full restore plan preserved at $Backup"
    throw
}
