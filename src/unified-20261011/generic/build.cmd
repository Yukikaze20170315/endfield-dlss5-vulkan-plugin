@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
set "OUT=%~dp0..\..\..\builds\unified-20261011\generic"
if not exist "%OUT%" mkdir "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
set "DEPS=F:\Users\Administrator\Documents\dlss5-multistage-dev\fix-20261004\renodx\external"
rc /nologo /fo "%OUT%\version.res" generated\resource.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX /DNDEBUG /I generated /I "F:\Users\Administrator\Documents\dlss5-multistage-dev\toolchain\dlss-sdk\include" /I "%DEPS%\Detours\include" /I "%DEPS%\json\include" /I "%DEPS%\frozen\include" /I "%DEPS%\gtl\include" /I "%DEPS%\Streamline\include" /I "%DEPS%\DLSS\include" /I "%DEPS%\reshade" /LD src\addons\dlss5\addon.cpp /Fo"%OUT%\addon.obj" /Fe"%OUT%\renodx-dlss5.addon64" /link "%OUT%\version.res" "%DEPS%\Detours\lib.X64\detours.lib" kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib bcrypt.lib /INCREMENTAL:NO
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
