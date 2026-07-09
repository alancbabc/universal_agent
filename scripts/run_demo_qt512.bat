@echo off
setlocal

set "ROOT=%~dp0.."
set "QT_PREFIX=%ROOT%\third_party\Qt\5.12.12\msvc2017_64"
set "BUILD_DIR=%ROOT%\build_nmake_qt512"
if not "%~1"=="" set "BUILD_DIR=%~1"

set "PATH=%QT_PREFIX%\bin;%BUILD_DIR%;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%QT_PREFIX%\plugins\platforms"
set "QT_PLUGIN_PATH=%QT_PREFIX%\plugins"

cd /d "%ROOT%"
start "VI Agent Demo" "%BUILD_DIR%\vi_agent_demo.exe"