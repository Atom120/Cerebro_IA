@echo off
rem Builds the ApertureRunner mod for Win32 (the architecture of Portal) and runs every test.
rem Usage (from any folder):  mod\build.bat            Release
rem                           mod\build.bat Debug      Debug
rem Output: build\x86\  (ignored by git)
setlocal

set "HERE=%~dp0"
set "ROOT=%HERE%.."
set "CONFIG=Release"
if not "%~1"=="" set "CONFIG=%~1"

rem Use cmake from PATH, otherwise the one bundled with Visual Studio.
where cmake >nul 2>nul
if errorlevel 1 (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE=%%i"
) else (
    set "CMAKE=cmake"
)
if not defined CMAKE (
    echo cmake was not found. Install Visual Studio with "Desktop development with C++".
    exit /b 1
)
for %%F in ("%CMAKE%") do set "CTEST=%%~dpFctest.exe"
if "%CMAKE%"=="cmake" set "CTEST=ctest"

"%CMAKE%" -S "%HERE%." -B "%ROOT%\build\x86" -A Win32 || exit /b 1
"%CMAKE%" --build "%ROOT%\build\x86" --config %CONFIG% || exit /b 1
"%CTEST%" --test-dir "%ROOT%\build\x86" -C %CONFIG% --output-on-failure || exit /b 1
echo.
echo OK: build\x86 (%CONFIG%, Win32)
