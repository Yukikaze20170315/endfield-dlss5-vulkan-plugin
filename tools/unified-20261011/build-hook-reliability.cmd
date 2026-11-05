@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\mfg-hook-reliability1n"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
set "DEPS=%ROOT%\src\unified-20261011\mfg\deps"
set "INC=/I"%DEPS%\reshade" /I"%DEPS%\streamline" /I"%DEPS%\ngx" /I"%DEPS%\nvapi" /I"%DEPS%\detours\include""
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /O2 /MT /bigobj /DNOMINMAX %INC% "%~dp0test-hook-reliability.cpp" /Fo:test-hook-reliability.obj /Fe:test-hook-reliability.exe /link "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
".\test-hook-reliability.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
