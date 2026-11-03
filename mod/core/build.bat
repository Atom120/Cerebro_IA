@echo off
rem Builds ApertureRunner.Core tests and tools for x64 and x86 with MSVC and runs the tests.
rem Usage: build.bat            (both architectures)
rem        build.bat x86        (only x86, the architecture of portal2.exe and MirrorsEdge.exe)
setlocal enabledelayedexpansion

set "HERE=%~dp0"
rem vcvars*.bat calls vswhere by name, so the installer folder must be on PATH.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
    echo Visual Studio with the C++ toolset was not found.
    exit /b 1
)

set "ARCHES=x64 x86"
if not "%~1"=="" set "ARCHES=%~1"

set "FAILED=0"
for %%A in (%ARCHES%) do (
    echo ===== %%A =====
    set "OUT=%HERE%build\%%A"
    if not exist "!OUT!" mkdir "!OUT!"
    if "%%A"=="x86" (set "VCVARS=vcvars32.bat") else (set "VCVARS=vcvars64.bat")
    cmd /c ""%VSROOT%\VC\Auxiliary\Build\!VCVARS!" >nul && cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I"%HERE%include" "%HERE%src\shared_arena.cpp" "%HERE%src\bridge.cpp" "%HERE%tests\test_core.cpp" /Fo"!OUT!\\" /Fe"!OUT!\test_core.exe" && cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I"%HERE%include" "%HERE%src\shared_arena.cpp" "%HERE%tools\ar_tool.cpp" /Fo"!OUT!\\" /Fe"!OUT!\ar_tool.exe""
    if errorlevel 1 (
        echo Build failed for %%A
        set "FAILED=1"
    ) else (
        "!OUT!\test_core.exe"
        if errorlevel 1 set "FAILED=1"
    )
)
exit /b %FAILED%
