@echo off
rem Focus/guard regression against the production source. No game or driver settings are changed.
rem Usage: run-focus-guard.cmd focus^|guard NEW_OUTPUT_DIRECTORY [SL_DLSS_G_DLL]
setlocal
if "%~2"=="" exit /b 2
if /i not "%~1"=="focus" if /i not "%~1"=="guard" exit /b 2
if /i "%~1"=="focus" if "%~3"=="" exit /b 2
set "OUT=%~f2"
set "RUNTIME=%~f3"
if exist "%OUT%" ( echo Output already exists: "%OUT%" & exit /b 2 )
call "%~dp0..\vulkan-fg-input\vcvars.cmd" || exit /b 2
for %%I in ("%~dp0..\..\src") do set "SRC=%%~fI"
for %%I in ("%~dp0..\..\..\..\..") do set "ROOT=%%~fI"
mkdir "%OUT%" || exit /b 2
pushd "%OUT%"
if /i "%~1"=="guard" goto guard
cl /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /EHsc /std:c++17 /I"%SRC%" "%~dp0test-focus.cpp" /Fe:test.exe /link user32.lib /OPT:NOICF >build.log 2>&1 || goto failed
test.exe "%RUNTIME%" >run.txt 2>&1
goto result
:guard
ml64 /nologo /c "%SRC%\present-nr-witness.asm" >build.log 2>&1 || goto failed
cl /nologo /W4 /WX /utf-8 /O2 /MT /EHsc /std:c++17 /I"%SRC%" /I"%SRC%\reshade" /I"%SRC%\minhook\include" /I"%SRC%\minhook\src" "%~dp0test-guard.cpp" "%SRC%\minhook.c" present-nr-witness.obj /Fe:test.exe /link user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib" >>build.log 2>&1 || goto failed
test.exe >run.txt 2>&1
:result
set "RESULT=%errorlevel%"
type run.txt
popd
exit /b %RESULT%
:failed
type build.log
popd
exit /b 2
