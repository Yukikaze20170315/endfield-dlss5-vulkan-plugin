@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "OUT=%~dp0..\..\builds\unified-20261011\small-stack-20261022"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
python "%~dp0prepare-generic-small-stack.py" generic-runtime-functions.inc || goto :fail
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT /I"%OUT%" "%~dp0test-generic-small-stack.cpp" /Fe:test-generic-small-stack.exe /link bcrypt.lib || goto :fail
test-generic-small-stack.exe > test-generic-small-stack-results.txt 2>&1
set "RESULT=%ERRORLEVEL%"
type test-generic-small-stack-results.txt
popd
exit /b %RESULT%
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
