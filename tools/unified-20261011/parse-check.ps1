# Syntax-check the PowerShell entry points without running them.
# Derives its own directory so the script holds no non-ASCII literals:
# Windows PowerShell 5.1 decodes a BOM-less file as ANSI and would garble them.
$ErrorActionPreference = 'Stop'
foreach ($name in @('deploy.ps1', 'test-deploy.ps1', 'DLSS5-Clean-Recovery.ps1', 'test-clean-recovery.ps1')) {
  $path = Join-Path $PSScriptRoot $name
  $tokens = $null
  $errors = $null
  [void][System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
  if ($errors.Count -gt 0) {
    Write-Output ("FAIL {0} ({1} error(s))" -f $name, $errors.Count)
    $errors | ForEach-Object { Write-Output ('  ' + $_.Message) }
  } else {
    Write-Output ("OK   {0}" -f $name)
  }
}
