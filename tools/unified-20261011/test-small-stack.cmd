@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "BRIDGE=%ROOT%\src\unified-20261011\bridge\src"
set "OUT=%ROOT%\builds\unified-20261011\small-stack-20261022"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT /LD "%~dp0fake-interposer.cpp" /Fe:sl.interposer.dll || goto :fail
ml64 /nologo /c "%BRIDGE%\present-nr-witness.asm" || goto :fail
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT /I"%BRIDGE%\reshade" /I"%BRIDGE%\minhook\include" /I"%BRIDGE%\minhook\src" "%~dp0test-small-stack.cpp" "%BRIDGE%\minhook.c" present-nr-witness.obj /Fe:test-small-stack.exe /link /MAP:test-small-stack.map /INCREMENTAL:NO user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib" || goto :fail
test-small-stack.exe > test-small-stack-results.txt 2>&1
set "RESULT=%ERRORLEVEL%"
type test-small-stack-results.txt
popd
exit /b %RESULT%
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
