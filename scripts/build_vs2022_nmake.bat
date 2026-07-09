@echo off
setlocal

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

set "QT_PREFIX=%~1"
if "%QT_PREFIX%"=="" set "QT_PREFIX=%ROOT%\third_party\Qt\5.12.12\msvc2017_64"
if not exist "%QT_PREFIX%\bin\qmake.exe" set "QT_PREFIX=%ROOT%\%QT_PREFIX%"
for %%I in ("%QT_PREFIX%") do set "QT_PREFIX=%%~fI"

set "BUILD_DIR=%ROOT%\build_nmake"
if not "%~2"=="" set "BUILD_DIR=%~2"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
for %%I in ("%BUILD_DIR%") do set "BUILD_DIR=%%~fI"

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

set "PATH=%QT_PREFIX%\bin;%PATH%"
set "QT_PLUGIN_PATH="
set "QT_QPA_PLATFORM_PLUGIN_PATH="

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "NMake Makefiles" -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b %errorlevel%

if exist "%QT_PREFIX%\bin\windeployqt.exe" (
    "%QT_PREFIX%\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%BUILD_DIR%\vi_agent_demo.exe"
    if errorlevel 1 exit /b %errorlevel%
)

echo.
echo Build finished:
echo   %BUILD_DIR%\vi_agent.dll
echo   %BUILD_DIR%\vi_agent_demo.exe
echo   %BUILD_DIR%\platforms\qwindows.dll