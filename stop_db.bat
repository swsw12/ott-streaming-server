@echo off
cd /d "%~dp0"
echo Stopping PostgreSQL Database...
".\pgsql\bin\pg_ctl.exe" -D ".\pgsql\data" stop
pause
