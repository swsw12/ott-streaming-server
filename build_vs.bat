@echo off
REM Find Visual Studio installation
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    echo [ERROR] Visual Studio not found!
    echo Please install Visual Studio 2022 with "Desktop development with C++" workload.
    pause
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64

REM Change to script directory
cd /d "%~dp0"

echo.
echo ========================================
echo Compiling OTT Server with PostgreSQL...
echo ========================================

cl /c /O2 /W3 /I"src" /D_CRT_SECURE_NO_WARNINGS src\main.c src\utils.c src\ffmpeg_helper.c src\http_handler.c src\db.c

if errorlevel 1 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Linking with libpq...
echo ========================================

link /OUT:ott_server.exe main.obj utils.obj ffmpeg_helper.obj http_handler.obj db.obj ws2_32.lib libpq.lib

if errorlevel 1 (
    echo Linking failed!
    echo.
    echo Make sure libpq.lib is in the current directory.
    echo Download from: https://www.postgresql.org/download/windows/
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Success!
echo ========================================
dir ott_server.exe
pause
