@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\pq-stale-event-audit"
if not exist "%OUT%\production-pq-wait.inc" exit /b 2
if exist "%OUT%\test-pq-stale-event.exe" exit /b 3
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT /I"%CD%" "%~dp0test-pq-stale-event.cpp" /Fe:test-pq-stale-event.exe /link d3d12.lib dxgi.lib
if errorlevel 1 exit /b %errorlevel%
test-pq-stale-event.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
