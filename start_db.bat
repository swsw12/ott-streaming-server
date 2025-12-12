@echo off
cd /d "%~dp0"

echo Checking if PostgreSQL is already running...
tasklist | find "postgres.exe" > nul
if %errorlevel% equ 0 (
    echo.
    echo PostgreSQL is already running! No need to start it again.
    pause
    exit /b 0
)

echo.
echo Starting PostgreSQL Database...
".\pgsql\bin\pg_ctl.exe" -D ".\pgsql\data" -l ".\pgsql\logfile" start
if errorlevel 1 (
    echo.
    echo Failed to start database. Please check permissions or logs.
) else (
    echo.
    echo Database started successfully!
)
pause
