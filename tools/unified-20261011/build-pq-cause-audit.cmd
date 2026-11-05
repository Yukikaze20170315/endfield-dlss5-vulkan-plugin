@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\pq-cause-audit1"
if not exist "%OUT%\source-manifest.json" exit /b 2
if exist "%OUT%\dlss5-bridge.addon64" exit /b 3
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
set "BRIDGE=%OUT%\source\bridge\src"
rc /nologo /fo version.res "%BRIDGE%\version.rc"
if errorlevel 1 exit /b %errorlevel%
ml64 /nologo /c "%BRIDGE%\present-nr-witness.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 /I"%BRIDGE%\reshade" /I"%BRIDGE%\minhook\include" /I"%BRIDGE%\minhook\src" /I"%ROOT%\src\unified-20261011\common" /LD "%BRIDGE%\dlss5-bridge.cpp" "%BRIDGE%\minhook.c" version.res present-nr-witness.obj /Fe:dlss5-bridge.addon64 /link /DLL user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT /DPQ_AUDIT_LOGGING /I"%CD%" "%~dp0test-pq-stale-event.cpp" /Fe:test-pq-stale-event.exe /link d3d12.lib dxgi.lib
if errorlevel 1 exit /b %errorlevel%
test-pq-stale-event.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
