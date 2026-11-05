@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\action1\action-tests"
if not exist "%OUT%" mkdir "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
pushd "%OUT%"
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 "%~dp0test-action-log.cpp" /Fe:test-action-log.exe
if errorlevel 1 goto :fail
test-action-log.exe "journal-%RANDOM%-%RANDOM%"
if errorlevel 1 goto :fail
popd
exit /b 0
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
