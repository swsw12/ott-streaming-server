@echo off
REM OTT Video Streaming Server - Windows Build Script
REM This script helps set up the build environment on Windows

echo ============================================
echo OTT Video Streaming Server - Build Setup
echo ============================================
echo.

REM Check for GCC (MinGW)
where gcc >nul 2>&1
if %errorlevel%==0 (
    echo [OK] GCC found
    goto :build
)

REM Check for MSVC
where cl >nul 2>&1
if %errorlevel%==0 (
    echo [OK] MSVC cl.exe found
    goto :build_msvc
)

echo [ERROR] No C compiler found!
echo.
echo Please install one of the following:
echo.
echo 1. MinGW-w64 (Recommended):
echo    - Download from: https://winlibs.com/
echo    - Or install via: winget install -e --id MingW-w64.MingW-w64
echo    - Add bin folder to PATH
echo.
echo 2. Visual Studio Build Tools:
echo    - Download from: https://visualstudio.microsoft.com/downloads/
echo    - Select "Desktop development with C++"
echo.
echo 3. WSL (Windows Subsystem for Linux):
echo    - wsl --install
echo    - wsl -d Ubuntu -- sudo apt update ^&^& sudo apt install build-essential
echo.
pause
exit /b 1

:build
echo.
echo Building with GCC...
gcc -o ott_server.exe src/main.c src/utils.c src/data.c src/ffmpeg_helper.c src/http_handler.c -lws2_32 -Wall -O2
if %errorlevel%==0 (
    echo.
    echo [SUCCESS] Build complete: ott_server.exe
    echo.
    echo Run with: ott_server.exe [port]
    echo Default port: 8080
) else (
    echo [ERROR] Build failed!
)
goto :end

:build_msvc
echo.
echo Building with MSVC...
cl /Fe:ott_server.exe src/main.c src/utils.c src/data.c src/ffmpeg_helper.c src/http_handler.c ws2_32.lib /O2 /W3
if %errorlevel%==0 (
    echo.
    echo [SUCCESS] Build complete: ott_server.exe
) else (
    echo [ERROR] Build failed!
)
goto :end

:end
echo.
pause
