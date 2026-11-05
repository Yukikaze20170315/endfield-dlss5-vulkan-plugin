[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Candidate,
    [string]$Target='dlss5-bridge.addon64',
    [string]$Game='C:\Program Files\Hypergryph Launcher\games\Endfield Game',
    [string]$EvidenceRoot,
    [Parameter(Mandatory=$true)][string]$Scope,
    [string]$ReferencePreserved
)
# Installs a single rebuilt add-on file over an existing installation. Used when a
# candidate is a one-file build and therefore not a complete deployment package.
# It never writes user settings, driver profiles or original game DLLs, and it
# refuses to run while the game is loaded.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
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
function Save-NewJson($Value,[string]$Path) {
    if(Test-Path -LiteralPath $Path) { throw "Existing evidence record: $Path" }
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
}

$Game=[IO.Path]::GetFullPath($Game).TrimEnd('\','/')
$Candidate=[IO.Path]::GetFullPath($Candidate)
$EvidenceRoot=[IO.Path]::GetFullPath($EvidenceRoot).TrimEnd('\','/')
foreach($path in @($Game,$Candidate,$EvidenceRoot)) { Assert-Plain $path }
if(!(Test-Path -LiteralPath (Join-Path $Game 'Endfield.exe'))) { throw 'Not an Endfield directory' }
if(!(Test-Path -LiteralPath $Candidate)) { throw "Candidate not found: $Candidate" }
$targetPath=Join-Path $Game $Target
if(!(Test-Path -LiteralPath $targetPath)) { throw "Target not installed: $targetPath" }
Closed

$before=Hash $targetPath
$after=Hash $Candidate
if($before -eq $after) { Write-Output 'Target already matches the candidate; no changes made.'; return }

# Protected inventory: everything the deployment must leave untouched. Mirrors the
# package files plus the game's own Streamline/NGX runtime and the user configs.
$protected=@()
foreach($name in @('DLSS5-使用与恢复 Usage and Recovery.md','DLSS5-Clean-Recovery.ps1','DLSS5-Recovery 故障恢复.cmd',
                   'DLSS5-RestoreDriver.exe','payload-manifest.json','release-manifest.json','SHA256SUMS.txt',
                   'renodx-dlss5.addon64','renodx-mfgunlock.addon64','ReShade.ini','dlss5-bridge.cfg')) {
    $p=Join-Path $Game $name
    if(Test-Path -LiteralPath $p) { $protected+=@{path=$name;sha256=(Hash $p)} }
}
foreach($dir in @('dlss5\nr\rtx50','dlss5\nr\rtx20-40','dlss5\sl-2.14.1')) {
    $d=Join-Path $Game $dir
    if(Test-Path -LiteralPath $d) {
        foreach($f in @(Get-ChildItem -LiteralPath $d -File)) {
            $protected+=@{path=($dir+'\'+$f.Name);sha256=(Hash $f.FullName)}
        }
    }
}
if($ReferencePreserved) {
    $ref=Get-Content -LiteralPath $ReferencePreserved -Raw -Encoding UTF8 | ConvertFrom-Json
    $refMap=@{}
    foreach($e in $ref.preserved) { $refMap[$e.path]=$e.sha256 }
    foreach($e in $protected) {
        if($refMap.ContainsKey($e.path) -and $refMap[$e.path] -ne $e.sha256) {
            throw "Protected file differs from reference: $($e.path)"
        }
    }
}

$Backup=Join-Path $EvidenceRoot ('install-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
[IO.Directory]::CreateDirectory($Backup) | Out-Null
Copy-Verified $targetPath (Join-Path $Backup $Target) $before
foreach($name in @('ReShade.ini','dlss5-bridge.cfg')) {
    $p=Join-Path $Game $name
    if(Test-Path -LiteralPath $p) {
        Copy-Verified $p (Join-Path $Backup $name) (Hash $p)
    }
}
Save-NewJson @{prepared_at=[DateTime]::Now.ToString('o');target=$Target;before=$before;after=$after;
               candidate=$Candidate;backup=$Backup;preserved=$protected;
               ini_absent=(!(Test-Path -LiteralPath (Join-Path $Game 'vk-present-adapter.ini')));scope=$Scope} `
             (Join-Path $Backup 'deployment.json')
Write-Output "Verified backup: $Backup"

$staged=$targetPath+'.install-'+[guid]::NewGuid().ToString('N')
Copy-Verified $Candidate $staged $after
Closed
[IO.File]::Replace($staged,$targetPath,[NullString]::Value)
if((Hash $targetPath) -ne $after) { throw "Installed hash mismatch: $targetPath" }
foreach($entry in $protected) {
    if((Hash (Join-Path $Game $entry.path)) -ne $entry.sha256) { throw "Protected file changed: $($entry.path)" }
}
Save-NewJson @{installed_at=[DateTime]::Now.ToString('o');backup=$Backup;changed_files=1;
               protected_files_verified=$protected.Count;installed_sha256=$after;
               ini_absent=(!(Test-Path -LiteralPath (Join-Path $Game 'vk-present-adapter.ini')))} `
             (Join-Path $Backup 'success.json')
Write-Output "Installed $Target. Protected hashes unchanged: $($protected.Count)."
