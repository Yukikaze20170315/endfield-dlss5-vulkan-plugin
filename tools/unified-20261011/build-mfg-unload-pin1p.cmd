@echo off
rem Builds MFG Unlock unload-pin1p from its isolated snapshot, reruns the source
rem policy fixtures, and runs the unload-race fixture against 1n and 1p.
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\mfg-unload-pin1p"
set "OLD=%ROOT%\builds\unified-20261011\mfg-hook-reliability1n"
set "DEPS=%ROOT%\src\unified-20261011\mfg\deps"
set "INC=/I"%DEPS%\reshade" /I"%DEPS%\streamline" /I"%DEPS%\ngx" /I"%DEPS%\nvapi" /I"%DEPS%\detours\include""
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
rc /nologo /fo version.res source\mfg\version.rc
if errorlevel 1 goto :fail
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /O2 /MT /bigobj /DNOMINMAX %INC% /LD source\mfg\addon\addon.cpp /Fo:addon.obj /Fe:renodx-mfgunlock.addon64 /link version.res "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 goto :fail
rem The policy fixture re-includes the production headers, which carry a
rem pre-existing C4459 shadowing warning; the shipped DLL is the /W4 /WX build.
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% source\mfg\tests\native-policy.cpp /Fo:native-policy.obj /Fe:native-policy.exe /link "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 goto :fail
".\native-policy.exe"
if errorlevel 1 goto :fail
if not exist startup-policy mkdir startup-policy
copy /y native-policy.exe startup-policy\Endfield.exe >nul
"startup-policy\Endfield.exe" --startup-policy
if errorlevel 1 goto :fail

if not exist race-fixture mkdir race-fixture
if not exist race-fixture\1n mkdir race-fixture\1n
if not exist race-fixture\1p mkdir race-fixture\1p
copy /y "%OLD%\renodx-mfgunlock.addon64" race-fixture\1n\renodx-mfgunlock.addon64 >nul
copy /y renodx-mfgunlock.addon64 race-fixture\1p\renodx-mfgunlock.addon64 >nul
pushd race-fixture
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /O2 /MT "%~dp0test-mfg-unload-race.cpp" /Fo:race-host.obj /Fe:Endfield.exe /link /INCREMENTAL:NO
if errorlevel 1 goto :failfix
cl /nologo /std:c++20 /W4 /WX /O2 /MT /LD /DMFG_RACE_GATE "%~dp0test-mfg-unload-race-dll.cpp" /Fo:gate.obj /Fe:mfg-race-gate.dll /link /INCREMENTAL:NO
if errorlevel 1 goto :failfix
cl /nologo /std:c++20 /W4 /WX /O2 /MT /LD "%~dp0test-mfg-unload-race-dll.cpp" /Fo:victim.obj /Fe:mfg-race-victim.dll /link /INCREMENTAL:NO
if errorlevel 1 goto :failfix
popd
popd
exit /b 0
:failfix
popd
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
