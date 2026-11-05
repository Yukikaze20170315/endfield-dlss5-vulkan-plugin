@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "OUT=%~dp0..\..\builds\unified-20261011\recovery"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
cl /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /O2 /MT "%~dp0restore-driver.cpp" /Fe:DLSS5-RestoreDriver.exe /link bcrypt.lib advapi32.lib "%~dp0..\..\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
