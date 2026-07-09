@echo off
setlocal

set "ROOT=%~dp0.."
set "DB=%ROOT%\database\test_vi_agent.db"
set "SCHEMA=%ROOT%\database\test_schema.sql"
set "SEED=%ROOT%\database\test_seed.sql"
set "SQLITE=sqlite3"

where sqlite3 >nul 2>nul
if errorlevel 1 (
    if exist "D:\anaconda3\Library\bin\sqlite3.exe" (
        set "SQLITE=D:\anaconda3\Library\bin\sqlite3.exe"
    )
)

echo [vi_agent] Generating test database:
echo   %DB%

"%SQLITE%" "%DB%" ".read %SCHEMA%" ".read %SEED%"
if errorlevel 1 (
    echo [vi_agent] Failed to generate database. Please install sqlite3 CLI or adjust SQLITE path.
    exit /b 1
)

echo [vi_agent] Done.
