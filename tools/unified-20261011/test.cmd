@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%
set "OUT=%~dp0..\..\builds\unified-20261011\tests"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"
python "%~dp0check-lang-strings.py"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /LD /utf-8 /O2 /MT "%~dp0test-crash-filter-dll.cpp" /Fe:test-crash-filter-dll.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /O2 /MT "%~dp0test-crash-filter-chain.cpp" /Fe:test-crash-filter-chain.exe
if errorlevel 1 exit /b %errorlevel%
test-crash-filter-chain.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /c "%~dp0test-ngx-info.cpp" /Fo:test-ngx-info.obj
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /DNOMINMAX "%~dp0test-state.cpp" /Fe:test-state.exe /link bcrypt.lib
if errorlevel 1 exit /b %errorlevel%
test-state.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /O2 /MT /LD "%~dp0fake-interposer.cpp" /Fe:sl.interposer.dll
if errorlevel 1 exit /b %errorlevel%
set "BRIDGE=%~dp0..\..\src\unified-20261011\bridge\src"
ml64 /nologo /c "%BRIDGE%\present-nr-witness.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /I"%BRIDGE%\reshade" /I"%BRIDGE%\minhook\include" /I"%BRIDGE%\minhook\src" "%~dp0test-loader.cpp" "%BRIDGE%\minhook.c" present-nr-witness.obj /Fe:test-loader.exe /link user32.lib advapi32.lib bcrypt.lib "%~dp0..\..\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib"
if errorlevel 1 exit /b %errorlevel%
test-loader.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
