@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\thread-lifetime-audit"
if exist "%OUT%" exit /b 1
mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /DNOMINMAX /I"%ROOT%\src\unified-20261011\mfg\deps\reshade" /I"%ROOT%\src\unified-20261011\mfg\deps\detours\include" "%~dp0test-thread-lifetime.cpp" /Fe:test-thread-lifetime.exe /link "%ROOT%\src\unified-20261011\mfg\deps\detours\lib.X64\detours.lib" user32.lib
if errorlevel 1 exit /b %errorlevel%
".\test-thread-lifetime.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
