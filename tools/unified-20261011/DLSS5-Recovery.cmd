@echo off
rem DLSS5 NR uninstaller launcher; all prompts live in DLSS5-Clean-Recovery.ps1.
rem Keep this file ASCII-only and free of labels and goto jumps: cmd.exe can land on
rem the wrong label in UTF-8 batch files. The uninstaller also archives this
rem file while it runs; "(goto) 2>nul" ends the batch context on the already
rem parsed last line, so cmd.exe never rereads the moved file.
setlocal DisableDelayedExpansion
cd /d "%~dp0" || exit /b 1
if not exist "%~dp0DLSS5-Clean-Recovery.ps1" (echo DLSS5-Clean-Recovery.ps1 is missing from this folder. Extract it again from the package. & pause & exit /b 1)
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0DLSS5-Clean-Recovery.ps1" -Action Uninstall -Game "%~dp0." & (goto) 2>nul & exit /b
