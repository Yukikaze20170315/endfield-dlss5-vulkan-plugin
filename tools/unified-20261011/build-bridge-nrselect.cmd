@echo off
rem Rebuild dlss5-bridge from the current source tree into a build output
rem directory, so a change lands as a new artifact without touching the source
rem tree's own copy. Mirrors src/unified-20261011/bridge/src/build.cmd.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "SRC=%ROOT%\src\unified-20261011\bridge\src"
set "OUT=%ROOT%\builds\unified-20261011\nr-runtime-select1o"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
rc /nologo /fo version.res "%SRC%\version.rc"
if errorlevel 1 exit /b 1
ml64 /nologo /c /Fo present-nr-witness.obj "%SRC%\present-nr-witness.asm"
if errorlevel 1 exit /b 1
if not exist present-nr-witness.obj exit /b 1
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 ^
   /I"%SRC%\reshade" /I"%SRC%\minhook\include" /I"%SRC%\minhook\src" /LD ^
   "%SRC%\dlss5-bridge.cpp" "%SRC%\minhook.c" version.res present-nr-witness.obj ^
   /Fe:dlss5-bridge.addon64 ^
   /link /DLL user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
if errorlevel 1 exit /b 1
popd
echo built: %OUT%\dlss5-bridge.addon64
endlocal
