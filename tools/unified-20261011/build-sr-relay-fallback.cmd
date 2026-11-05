@echo off
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\sr-relay-tests"
set "MH=%ROOT%\src\unified-20261011\bridge\src"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /LD "%~dp0sr-relay-fixture.cpp" ^
   /Fe:sr-relay-fixture.dll /link /FIXED /BASE:0x6EDD0000 /INCREMENTAL:NO || goto :fail
cl /nologo /W4 /O2 /MT /TC /c /I"%MH%" /I"%MH%\minhook\src" "%MH%\minhook.c" /Fo:minhook.obj || goto :fail
cl /nologo /W4 /O2 /MT /EHsc /std:c++17 /utf-8 /I"%MH%\minhook\include" ^
   "%~dp0test-sr-relay-fallback.cpp" minhook.obj /Fe:test-sr-relay-fallback.exe || goto :fail
".\test-sr-relay-fallback.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
:fail
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
