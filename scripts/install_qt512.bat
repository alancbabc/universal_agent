@echo off
setlocal

set "ROOT=%~dp0.."
set "QT_ROOT=%ROOT%\third_party\Qt"
set "QT_PREFIX=%QT_ROOT%\5.12.12\msvc2017_64"

echo [vi_agent] Installing Qt 5.12.12 win64_msvc2017_64 into:
echo   %QT_ROOT%

python -m pip show aqtinstall >nul 2>nul
if errorlevel 1 (
    echo [vi_agent] aqtinstall not found, installing with pip...
    python -m pip install --user aqtinstall
    if errorlevel 1 exit /b 1
)

if exist "%QT_PREFIX%\bin\qmake.exe" (
    echo [vi_agent] Qt already exists:
    "%QT_PREFIX%\bin\qmake.exe" -v
    exit /b 0
)

python -m aqt install-qt windows desktop 5.12.12 win64_msvc2017_64 --outputdir "%QT_ROOT%"
if errorlevel 1 exit /b 1

"%QT_PREFIX%\bin\qmake.exe" -v
