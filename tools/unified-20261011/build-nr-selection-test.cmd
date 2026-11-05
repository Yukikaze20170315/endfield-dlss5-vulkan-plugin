@echo off
rem Compile and run the NR-runtime selection fixture. It includes the generated
rem payload.hpp, so it also fails if payload generation dropped the table or
rem produced a header that does not compile.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\builds\unified-20261011\nr-selection-tests"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "TEMP=%CD%"
set "TMP=%CD%"
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /O2 /MT /I"%ROOT%\src\unified-20261011" ^
   "%~dp0test-nr-selection.cpp" /Fe:test-nr-selection.exe
if errorlevel 1 exit /b %errorlevel%
".\test-nr-selection.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
