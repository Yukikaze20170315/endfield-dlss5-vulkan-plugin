@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\hook-cause-audit1"
if exist "%OUT%\renodx-mfgunlock.addon64" exit /b 1
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
set "DEPS=%ROOT%\src\unified-20261011\mfg\deps"
set "INC=/I"%DEPS%\reshade" /I"%DEPS%\streamline" /I"%DEPS%\ngx" /I"%DEPS%\nvapi" /I"%DEPS%\detours\include""
rc /nologo /fo version.res source\mfg\version.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% /LD source\mfg\addon\addon.cpp /Fo:addon.obj /Fe:renodx-mfgunlock.addon64 /link version.res "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% source\mfg\tests\native-policy.cpp /Fo:native-policy.obj /Fe:native-policy.exe /link "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
native-policy.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% "%~dp0test-hook-cause-audit.cpp" /Fo:test-hook-cause-audit.obj /Fe:test-hook-cause-audit.exe /link "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
test-hook-cause-audit.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
