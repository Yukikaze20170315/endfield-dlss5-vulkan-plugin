@echo off
rem Builds the unified1q add-ons from the production source tree.
rem   build-unified1q.cmd [bridge|mfg|generic|tests|all] [output-subdirectory]
rem Outputs go to builds\unified-20261011\<output-subdirectory> (default unified1q).
rem The MFG line mirrors build-mfg-unload-pin1p.cmd (static detours.lib, no
rem small-block object); the Generic line mirrors generic\build.cmd.
setlocal
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "WHAT=%~1"
if "%WHAT%"=="" set "WHAT=all"
set "SUB=%~2"
if "%SUB%"=="" set "SUB=unified1q"
set "ROOT=%~dp0..\.."
set "SRC=%ROOT%\src\unified-20261011"
set "OUT=%ROOT%\builds\unified-20261011\%SUB%"
if not exist "%OUT%" mkdir "%OUT%"
set "TEMP=%OUT%"
set "TMP=%OUT%"

if /i "%WHAT%"=="bridge" goto :bridge
if /i "%WHAT%"=="mfg" goto :mfg
if /i "%WHAT%"=="generic" goto :generic
if /i "%WHAT%"=="tests" goto :tests

:bridge
if not exist "%OUT%\bridge" mkdir "%OUT%\bridge"
pushd "%OUT%\bridge"
rc /nologo /fo version.res "%SRC%\bridge\src\version.rc" || goto :fail
ml64 /nologo /c /Fo present-nr-witness.obj "%SRC%\bridge\src\present-nr-witness.asm" || goto :fail
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 ^
   /I"%SRC%\bridge\src\reshade" /I"%SRC%\bridge\src\minhook\include" /I"%SRC%\bridge\src\minhook\src" /LD ^
   "%SRC%\bridge\src\dlss5-bridge.cpp" "%SRC%\bridge\src\minhook.c" version.res present-nr-witness.obj ^
   /Fe:dlss5-bridge.addon64 ^
   /link /DLL user32.lib advapi32.lib bcrypt.lib "%ROOT%\upstream\NVIDIA-nvapi-87dca62\amd64\nvapi64.lib" || goto :fail
popd
if /i "%WHAT%"=="bridge" goto :done

:mfg
if not exist "%OUT%\mfg" mkdir "%OUT%\mfg"
set "DEPS=%SRC%\mfg\deps"
set "INC=/I"%DEPS%\reshade" /I"%DEPS%\streamline" /I"%DEPS%\ngx" /I"%DEPS%\nvapi" /I"%DEPS%\detours\include""
pushd "%OUT%\mfg"
rc /nologo /fo version.res "%SRC%\mfg\version.rc" || goto :fail
cl /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /O2 /MT /bigobj /DNOMINMAX %INC% /LD "%SRC%\mfg\addon\addon.cpp" /Fo:addon.obj /Fe:renodx-mfgunlock.addon64 /link version.res "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO || goto :fail
rem The policy fixture re-includes the production headers, which carry a
rem pre-existing C4459 shadowing warning; the shipped DLL is the /W4 /WX build.
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX %INC% "%SRC%\mfg\tests\native-policy.cpp" /Fo:native-policy.obj /Fe:native-policy.exe /link "%DEPS%\detours\lib.X64\detours.lib" version.lib user32.lib advapi32.lib /INCREMENTAL:NO || goto :fail
".\native-policy.exe" || goto :fail
if not exist startup-policy mkdir startup-policy
copy /y native-policy.exe startup-policy\Endfield.exe >nul
"startup-policy\Endfield.exe" --startup-policy || goto :fail
popd
if /i "%WHAT%"=="mfg" goto :done

:generic
if not exist "%OUT%\generic" mkdir "%OUT%\generic"
set "GDEPS=F:\Users\Administrator\Documents\dlss5-multistage-dev\fix-20261004\renodx\external"
pushd "%SRC%\generic"
rc /nologo /fo "%OUT%\generic\version.res" generated\resource.rc || goto :failg
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MT /bigobj /DNOMINMAX /DNDEBUG /I generated /I "F:\Users\Administrator\Documents\dlss5-multistage-dev\toolchain\dlss-sdk\include" /I "%GDEPS%\Detours\include" /I "%GDEPS%\json\include" /I "%GDEPS%\frozen\include" /I "%GDEPS%\gtl\include" /I "%GDEPS%\Streamline\include" /I "%GDEPS%\DLSS\include" /I "%GDEPS%\reshade" /LD src\addons\dlss5\addon.cpp /Fo"%OUT%\generic\addon.obj" /Fe"%OUT%\generic\renodx-dlss5.addon64" /link "%OUT%\generic\version.res" "%GDEPS%\Detours\lib.X64\detours.lib" kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib bcrypt.lib /INCREMENTAL:NO || goto :failg
popd
if /i "%WHAT%"=="generic" goto :done

:tests
if not exist "%OUT%\tests" mkdir "%OUT%\tests"
pushd "%OUT%\tests"
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 "%~dp0test-event-log.cpp" /Fe:test-event-log.exe /link advapi32.lib || goto :fail
".\test-event-log.exe" || goto :fail
rem Real Bridge DLL in a temporary game folder (safe mode, fake ReShade).
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 /LD /DFAKE_RESHADE "%~dp0test-bridge-logdir.cpp" /Fo:fake-reshade.obj /Fe:fake-reshade.dll || goto :fail
cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 "%~dp0test-bridge-logdir.cpp" /Fo:test-bridge-logdir.obj /Fe:test-bridge-logdir.exe || goto :fail
".\test-bridge-logdir.exe" "%OUT%\bridge\dlss5-bridge.addon64" || goto :fail
popd
goto :done

:failg
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
:fail
set "RESULT=%ERRORLEVEL%"
if "%RESULT%"=="0" set "RESULT=1"
popd
exit /b %RESULT%
:done
echo build-unified1q: %WHAT% done in %OUT%
endlocal
exit /b 0
