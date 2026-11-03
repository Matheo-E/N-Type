@echo off
setlocal
cd /d "%~dp0"
set "PRESET=windows-release"
set "CLEAN=0"

:parse
if "%~1"=="" goto setup
if /i "%~1"=="--debug" (
    set "PRESET=windows-debug"
    shift
    goto parse
)
if /i "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto parse
)
if /i "%~1"=="--help" goto help
if /i "%~1"=="-h" goto help
echo Unknown option: %~1
exit /b 1

:help
echo Usage: build.bat [--debug] [--clean] [--help]
exit /b 0

:setup
rem Preserve a user's vcpkg path before Visual Studio changes its environment.
set "CLIENT_VCPKG_ROOT=%VCPKG_ROOT%"
where cl >nul 2>nul
if not errorlevel 1 goto compiler_ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_compiler
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_compiler
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

:compiler_ready
for %%t in (git cmake ninja) do (
    where %%t >nul 2>nul || (echo Missing tool: %%t & exit /b 1)
)
set "VCPKG_ROOT=%CLIENT_VCPKG_ROOT%"
if defined VCPKG_ROOT goto bootstrap
set "VCPKG_ROOT=%CD%\vcpkg"
if exist "%VCPKG_ROOT%\.git" goto bootstrap
git clone --filter=tree:0 https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
if errorlevel 1 exit /b 1

:bootstrap
if exist "%VCPKG_ROOT%\vcpkg.exe" goto build
call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
if errorlevel 1 exit /b 1

:build
if "%CLEAN%"=="1" if exist "build\client-%PRESET%" rmdir /s /q "build\client-%PRESET%"
cmake --preset %PRESET%
if errorlevel 1 exit /b 1
cmake --build --preset %PRESET%
if errorlevel 1 exit /b 1
copy /y "build\client-%PRESET%\bin\r-type_client.exe" . >nul
if errorlevel 1 exit /b 1
copy /y "build\client-%PRESET%\bin\r-type_server.exe" . >nul
if errorlevel 1 exit /b 1
rem Keep the runtime DLLs next to the executables.
if exist "build\client-%PRESET%\bin\*.dll" copy /y "build\client-%PRESET%\bin\*.dll" . >nul
if errorlevel 1 exit /b 1
echo Run r-type_server.exe, then r-type_client.exe
exit /b 0

:no_compiler
echo Install Visual Studio 2022 or newer with the Desktop development with C++ workload.
exit /b 1
