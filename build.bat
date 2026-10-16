@echo off
rem Builds N-Type and copies r-type_server and r-type_client to the repository root.
rem
rem Uses %VCPKG_ROOT% when it is set; otherwise fetches vcpkg once into .\vcpkg.
rem Needs Visual Studio 2022 (or newer) with the "Desktop development with C++"
rem workload, which provides the compiler, CMake and Ninja.
rem Run build.bat --help for the options.

setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "PRESET=windows-release"
set "CLEAN=0"

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--debug" set "PRESET=windows-debug" & shift & goto parse
if /i "%~1"=="--clean" set "CLEAN=1" & shift & goto parse
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage
echo build.bat: unknown option '%~1' 1>&2
call :print_usage 1>&2
exit /b 1

:usage
call :print_usage
exit /b 0

:print_usage
echo USAGE: build.bat [options]
echo   --debug     debug build (default: release)
echo   --clean     delete the previous build of the chosen configuration first
echo   -h, --help  show this help
exit /b 0

:parsed
rem Remember the user's vcpkg: the Visual Studio environment script below
rem replaces VCPKG_ROOT with the copy bundled with Visual Studio.
set "USER_VCPKG_ROOT=%VCPKG_ROOT%"

where cl >nul 2>nul
if not errorlevel 1 goto have_compiler
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_visual_studio
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_visual_studio
echo -- Loading the Visual Studio environment from %VSDIR%
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto no_visual_studio

:have_compiler
set "VCPKG_ROOT=%USER_VCPKG_ROOT%"
for %%t in (git cmake ninja) do (
    where %%t >nul 2>nul || (echo build.bat: '%%t' is required but not found 1>&2 & exit /b 1)
)

rem The pinned baseline in vcpkg.json needs a git checkout of vcpkg; the copy
rem bundled with Visual Studio (what a Developer Prompt points to) is not one.
if defined VCPKG_ROOT if not exist "%VCPKG_ROOT%\.git" (
    echo -- Ignoring VCPKG_ROOT=!VCPKG_ROOT!: not a git checkout of vcpkg
    set "VCPKG_ROOT="
)
if defined VCPKG_ROOT goto check_user_vcpkg
set "VCPKG_ROOT=%CD%\vcpkg"
if exist "%VCPKG_ROOT%\.git" goto bootstrap_vcpkg
echo -- Fetching vcpkg into .\vcpkg (first build only)
rem Commits only; vcpkg downloads the files it needs on demand.
git clone --filter=tree:0 https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
if errorlevel 1 exit /b 1

:bootstrap_vcpkg
if exist "%VCPKG_ROOT%\vcpkg.exe" goto vcpkg_ready
call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
if errorlevel 1 exit /b 1
goto vcpkg_ready

:check_user_vcpkg
if exist "%VCPKG_ROOT%\vcpkg.exe" goto vcpkg_ready
echo build.bat: VCPKG_ROOT=%VCPKG_ROOT% is not a bootstrapped vcpkg 1>&2
exit /b 1

:vcpkg_ready
if "%CLEAN%"=="1" if exist "build\%PRESET%" rmdir /s /q "build\%PRESET%"

echo -- Building with preset %PRESET% (the first build also compiles the dependencies)
cmake --preset %PRESET%
if errorlevel 1 exit /b 1
cmake --build --preset %PRESET%
if errorlevel 1 exit /b 1

rem The executables need the DLLs vcpkg copied next to them.
del /q r-type_server.exe r-type_client.exe 2>nul
copy /y "build\%PRESET%\bin\*.exe" . >nul
copy /y "build\%PRESET%\bin\*.dll" . >nul 2>nul
del /q ntype_tests.exe 2>nul

echo.
echo Done. Run from the repository root:
echo   r-type_server.exe -p 4242
echo   r-type_client.exe -h 127.0.0.1 -p 4242
exit /b 0

:no_visual_studio
echo build.bat: Visual Studio with the C++ tools was not found. 1>&2
echo Install Visual Studio 2022 or newer with the "Desktop development with C++" workload. 1>&2
exit /b 1
