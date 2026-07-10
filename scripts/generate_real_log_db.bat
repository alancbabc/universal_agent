@echo off
setlocal

set "ROOT=%~dp0.."
set "PYTHON=python"
set "LOG=%ROOT%\app_20260613_180017.log"
set "DB=%ROOT%\database\real_log_vi_agent.db"
set "SCHEMA=%ROOT%\database\real_log_schema_proposal.sql"

echo [vi_agent] Generating real log database:
echo   %DB%

%PYTHON% "%ROOT%\scripts\import_real_log_fast.py" --log "%LOG%" --db "%DB%" --schema "%SCHEMA%" --project-id "wuliangye_line_a"
if errorlevel 1 (
    echo [vi_agent] Failed to generate real log database.
    exit /b 1
)

echo [vi_agent] Done.

