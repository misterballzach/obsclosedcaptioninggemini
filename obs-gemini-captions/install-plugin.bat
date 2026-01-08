@echo off
echo ========================================================
echo OBS Gemini Captions - Installer
echo ========================================================
echo.

set BUILD_DLL=build\Release\obs-gemini-captions.dll
if not exist "%BUILD_DLL%" (
    echo [ERROR] Could not find plugin DLL at: %BUILD_DLL%
    echo Please run 'easy-build.bat' first!
    pause
    exit /b 1
)

echo Default OBS Path: C:\Program Files\obs-studio
set /p OBS_PATH="Enter OBS Path (Press Enter for default): "
if "%OBS_PATH%"=="" set OBS_PATH=C:\Program Files\obs-studio

if not exist "%OBS_PATH%\obs-plugins\64bit" (
    echo [ERROR] Could not find OBS plugins folder at: %OBS_PATH%\obs-plugins\64bit
    echo Please check your path.
    pause
    exit /b 1
)

echo.
echo Installing to: %OBS_PATH%
echo.

echo 1. Copying DLL...
copy /Y "%BUILD_DLL%" "%OBS_PATH%\obs-plugins\64bit\"
if %errorlevel% neq 0 (
    echo [ERROR] Copy failed. ACCESS DENIED.
    echo ********************************************************
    echo PLEASE RUN THIS SCRIPT AS ADMINISTRATOR!
    echo Right-click 'install-plugin.bat' -> Run as Administrator
    echo ********************************************************
    pause
    exit /b 1
)

echo 2. Copying Data...
set DATA_DEST=%OBS_PATH%\data\obs-plugins\obs-gemini-captions
if not exist "%DATA_DEST%" mkdir "%DATA_DEST%"
xcopy /E /I /Y "data\*" "%DATA_DEST%\"
if %errorlevel% neq 0 (
    echo [ERROR] Data copy failed. Access Denied?
    pause
    exit /b 1
)

echo.
echo ========================================================
echo [SUCCESS] Installation Complete!
echo ========================================================
echo Restart OBS Studio to load the plugin.
echo.
pause
