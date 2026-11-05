param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$CandidateHash
)
# Install the low-VRAM Generic candidate on the verified D-machine only.
# Requires the game to have exited. Configuration and other add-ons are unchanged.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$game = 'D:\Hypergryph Launcher\games\Endfield Game'
$name = 'renodx-dlss5.addon64'
$beforeHash = '353545DEF2070D7B73C5E645DDAC1872F71E43BEB3D3BC15E6C4E8E5691EC21A'
$source = Join-Path $PSScriptRoot $name
$destination = Join-Path $game $name
$backup = Join-Path $PSScriptRoot 'backup'
function Assert-Plain([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point: $($item.FullName)" }
        if ($item.PSIsContainer) { $item = $item.Parent } else { $item = $item.Directory }
    }
}
function Assert-Stopped {
    if (Get-Process Endfield -ErrorAction SilentlyContinue) { throw 'Endfield is running; exit normally first' }
}
function Assert-Hash([string]$Path,[string]$Hash) {
    Assert-Plain $Path
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) { throw "Hash mismatch: $Path" }
}
$os = Get-CimInstance Win32_OperatingSystem
if ($os.ProductType -ne 1 -or [int]$os.BuildNumber -lt 22000 -or $os.SystemDrive -ne 'C:') { throw 'Unexpected OS' }
foreach ($letter in @('C','D')) {
    if ((Get-Partition -DriveLetter $letter | Get-Disk).UniqueId -ne 'eui.002538B431B7BA1C') { throw "Unexpected disk: $letter" }
}
Assert-Plain $game
Assert-Plain $PSScriptRoot
Assert-Stopped
Assert-Hash $source $CandidateHash
Assert-Hash $destination $beforeHash
if ((Get-Content -LiteralPath (Join-Path $game 'dlss5-bridge.cfg') -TotalCount 1) -ne '# dlss5-bridge keep') { throw 'Keep marker missing' }
if (Test-Path -LiteralPath $backup) { throw 'Backup exists; refusing to overwrite it' }
New-Item -ItemType Directory -Path $backup | Out-Null
$preserved = [ordered]@{}
foreach ($file in @($name,'dlss5-bridge.addon64','renodx-mfgunlock.addon64','ReShade.ini','dlss5-bridge.cfg','vk-present-adapter.ini','renodx-mfgunlock.ini','ReShade.log')) {
    $path = Join-Path $game $file
    if (Test-Path -LiteralPath $path) {
        Assert-Plain $path
        $hash = (Get-FileHash -LiteralPath $path).Hash
        [IO.File]::Copy($path,(Join-Path $backup $file),$false)
        Assert-Hash (Join-Path $backup $file) $hash
        $preserved[$file] = $hash
    }
}
Assert-Stopped
Assert-Hash $destination $beforeHash
[IO.File]::Copy($source,$destination,$true)
Assert-Hash $destination $CandidateHash
foreach ($file in $preserved.Keys) {
    if ($file -ne $name) { Assert-Hash (Join-Path $game $file) $preserved[$file] }
}
$receipt = [ordered]@{Time=(Get-Date -Format o);Game=$game;Backup=$backup;Before=$beforeHash;After=$CandidateHash;Preserved=$preserved}
$json = $receipt | ConvertTo-Json -Depth 4
$path = Join-Path $PSScriptRoot 'deployment.json'
$stream = [IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try { $bytes=[Text.UTF8Encoding]::new($false).GetBytes($json);$stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
$json
