@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\throttle-default1k"
if exist "%OUT%" (
  echo Output already exists; preserve the previous build.
  exit /b 1
)
mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
set "BRIDGE=%ROOT%\src\unified-20261011\bridge\src"
rc /nologo /fo version.res "%BRIDGE%\version.rc"
if errorlevel 1 exit /b %errorlevel%
ml64 /nologo /c "%BRIDGE%\present-nr-witness.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 /I"%BRIDGE%\reshade" /I"%BRIDGE%\minhook\include" /I"%BRIDGE%\minhook\src" /LD "%BRIDGE%\dlss5-bridge.cpp" "%BRIDGE%\minhook.c" version.res present-nr-witness.obj /Fe:dlss5-bridge.addon64 /link /DLL user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /c "%~dp0test-ngx-info.cpp" /Fo:test-ngx-info.obj
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /DNOMINMAX "%~dp0test-state.cpp" /Fe:test-state.exe /link bcrypt.lib
if errorlevel 1 exit /b %errorlevel%
test-state.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /O2 /MT /LD "%~dp0fake-interposer.cpp" /Fe:sl.interposer.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /I"%BRIDGE%\reshade" /I"%BRIDGE%\minhook\include" /I"%BRIDGE%\minhook\src" "%~dp0test-loader.cpp" "%BRIDGE%\minhook.c" present-nr-witness.obj /Fe:test-loader.exe /link user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
if errorlevel 1 exit /b %errorlevel%
test-loader.exe
if errorlevel 1 exit /b %errorlevel%
python "%~dp0test-throttle-default.py"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
