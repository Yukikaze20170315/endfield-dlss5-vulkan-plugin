param([ValidateRange(1,600)][int]$Seconds=180)
# Read-only game/process observation; output is restricted to project evidence.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game='C:\Program Files\Hypergryph Launcher\games\Endfield Game'
$out=Join-Path $root ('evidence/unified-20261011/startup-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
if(Test-Path -LiteralPath $out){throw 'Evidence directory already exists'}
[IO.Directory]::CreateDirectory($out) | Out-Null
$record=Join-Path $out 'changes.jsonl'
$utf8=[Text.UTF8Encoding]::new($false)
$previous=''
$clock=[Diagnostics.Stopwatch]::StartNew()
Write-Output $out
while($clock.Elapsed.TotalSeconds -lt $Seconds){
    $pids=@(Get-Process -Name Endfield -ErrorAction SilentlyContinue | ForEach-Object Id)
    try {
        $data=[IO.File]::ReadAllBytes((Join-Path $game 'dlss5/state/bridge.state'))
        $key=([Convert]::ToBase64String($data))+'|'+($pids -join ',')
        if($key -ne $previous -and $data.Length -eq 80){
            $state=[ordered]@{at=[DateTime]::Now.ToString('o');pids=$pids;generation=[BitConverter]::ToUInt64($data,16)}
            $names=@('selected','confirmed','active','healthy','trial','failures','safe','nr','early','confirmed_nr','confirmed_early','reserved')
            for($i=0;$i -lt $names.Count;$i++){$state[$names[$i]]=[BitConverter]::ToUInt32($data,32+4*$i)}
            $line=$state | ConvertTo-Json -Compress
            [IO.File]::AppendAllText($record,$line+"`n",$utf8)
            Write-Output $line
            $previous=$key
        }
    } catch [IO.IOException] { }
    Start-Sleep -Milliseconds 250
}
