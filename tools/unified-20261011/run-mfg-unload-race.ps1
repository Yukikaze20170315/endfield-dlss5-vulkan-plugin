# Runs the MFG unload-race fixture (built by build-mfg-unload-pin1p.cmd) against
# the 1n and 1p add-on builds and writes a JSON summary next to the fixture.
# Expected: race/1n terminates with 0xC0000005 at offset 0x31183 every run;
# race/1p and cycle/1p exit 0.
param([int]$Runs = 5)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path $PSScriptRoot '..\..\builds\unified-20261011\mfg-unload-pin1p\race-fixture'
$fixture = (Resolve-Path -LiteralPath $fixture).Path
$host_exe = Join-Path $fixture 'Endfield.exe'
$results = @()
foreach ($mode in 'race', 'cycle') {
    foreach ($build in '1n', '1p') {
        $dll = Join-Path $fixture "$build\renodx-mfgunlock.addon64"
        $count = if ($mode -eq 'race') { $Runs } else { 1 }
        for ($i = 1; $i -le $count; ++$i) {
            $log = Join-Path $fixture "$mode-$build-$i.txt"
            $p = Start-Process -FilePath $host_exe -ArgumentList "$mode `"$dll`"" -WorkingDirectory $fixture `
                -NoNewWindow -Wait -PassThru -RedirectStandardOutput $log
            $line = (Select-String -LiteralPath $log -Pattern '^RESULT' | Select-Object -Last 1).Line
            $results += [pscustomobject]@{ mode = $mode; build = $build; run = $i
                exit = ('0x{0:X8}' -f $p.ExitCode); result = $line }
        }
    }
}
$results | Format-Table -AutoSize | Out-String -Width 220 | Write-Output
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'race-summary.json') -Encoding UTF8
