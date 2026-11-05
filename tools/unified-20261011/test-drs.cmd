@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "OUT=%~dp0..\..\builds\unified-20261011\tests-drs"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT "%~dp0test-drs.cpp" /Fe:test-drs.exe /link bcrypt.lib
if errorlevel 1 exit /b %errorlevel%
test-drs.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT "%~dp0test-fg-drs.cpp" /Fe:test-fg-drs.exe /link bcrypt.lib
if errorlevel 1 exit /b %errorlevel%
test-fg-drs.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT "%~dp0test-vram-budget.cpp" /Fe:test-vram-budget.exe /link bcrypt.lib advapi32.lib
if errorlevel 1 exit /b %errorlevel%
test-vram-budget.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT /LD "%~dp0fake-maintenance.cpp" /Fe:maintenance-probe.addon64 /link bcrypt.lib advapi32.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT "%~dp0test-maintenance.cpp" /Fe:test-maintenance.exe /link bcrypt.lib advapi32.lib
if errorlevel 1 exit /b %errorlevel%
test-maintenance.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
