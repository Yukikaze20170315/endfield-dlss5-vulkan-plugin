@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
set "OUT=%~dp0..\..\..\builds\unified-20261011\mfg-fragmentation1l"
if not exist "%OUT%" mkdir "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
set "INC=/I deps\reshade /I deps\streamline /I deps\ngx /I deps\nvapi /I deps\detours\include"
cl /nologo /c /std:c++17 /EHsc /O2 /MT /Ideps\detours\include deps\detours\src\detours.cpp /Fo"%OUT%\detours-small.obj"
if errorlevel 1 exit /b %errorlevel%
rc /nologo /fo "%OUT%\version.res" version.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% /LD addon\addon.cpp "%OUT%\detours-small.obj" /Fo"%OUT%\addon.obj" /Fe"%OUT%\renodx-mfgunlock.addon64" /link "%OUT%\version.res" deps\detours\lib.X64\detours.lib version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% tests\native-policy.cpp "%OUT%\detours-small.obj" /Fo"%OUT%\native-policy.obj" /Fe"%OUT%\native-policy.exe" /link deps\detours\lib.X64\detours.lib version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
"%OUT%\native-policy.exe"
if errorlevel 1 exit /b %errorlevel%
copy /b /y "%OUT%\native-policy.exe" "%OUT%\Endfield.exe" >nul
if errorlevel 1 exit /b %errorlevel%
"%OUT%\Endfield.exe" --startup-policy
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
