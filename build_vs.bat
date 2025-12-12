@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "c:\Users\ksw\Desktop\video_streaming"

echo.
echo ========================================
echo Compiling OTT Server with PostgreSQL...
echo ========================================

rem data.c is removed, replaced by db.c which implements the same interface
cl /c /O2 /W3 /I"src" /D_CRT_SECURE_NO_WARNINGS src\main.c src\utils.c src\ffmpeg_helper.c src\http_handler.c src\db.c

if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

echo.
echo ========================================
echo Linking with libpq...
echo ========================================

link /OUT:ott_server.exe main.obj utils.obj ffmpeg_helper.obj http_handler.obj db.obj ws2_32.lib libpq.lib

if errorlevel 1 (
    echo Linking failed!
    exit /b 1
)

echo.
echo ========================================
echo Build Success!
echo ========================================
dir ott_server.exe
