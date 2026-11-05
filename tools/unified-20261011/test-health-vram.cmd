@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "OUT=%~dp0..\..\builds\unified-20261011\health-vram-test"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
cl /nologo /W4 /WX /std:c++20 /EHsc /utf-8 /O2 /MT "%~dp0test-health-vram.cpp" /Fe:test-health-vram.exe || goto :fail
test-health-vram.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
