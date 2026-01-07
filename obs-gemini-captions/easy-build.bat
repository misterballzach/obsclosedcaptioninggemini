@echo off
echo ========================================================
echo OBS Gemini Captions - Easy Build Script
echo ========================================================
echo.

REM Check if CMake is available
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not found in your PATH.
    echo Please install CMake and ensure it is added to your PATH.
    pause
    exit /b 1
)

REM Optional: Ask for Qt Path
echo.
echo [OPTIONAL] Enter Qt6 directory if not in PATH.
echo Example: C:\Qt\6.6.3\msvc2019_64\lib\cmake\Qt6
echo (Press Enter to skip and try auto-detection)
echo.
set /p USER_QT_DIR="Qt Path: "

REM Create build directory
if not exist "build" (
    echo [INFO] Creating build directory...
    mkdir build
)

cd build

echo.
echo [INFO] Configuring Project (Auto-detecting Visual Studio)...
echo [NOTE] If this fails, ensure you have Visual Studio (C++ Desktop) installed.

set CMAKE_ARGS=-A x64
if not "%USER_QT_DIR%"=="" set CMAKE_ARGS=%CMAKE_ARGS% -DQt6_DIR="%USER_QT_DIR%"

cmake .. %CMAKE_ARGS%
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Configuration failed.
    echo 1. Check if Visual Studio with "Desktop development with C++" is installed.
    echo 2. Check if you ran 'setup-sdk.ps1' successfully.
    echo 3. Check if the Qt path you provided is correct (it should contain Qt6Config.cmake).
    cd ..
    pause
    exit /b 1
)

echo.
echo [INFO] Building Release Version...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================================
echo [SUCCESS] Build Complete!
echo ========================================================
echo.
echo The plugin DLL is located in: build\Release\
echo.
echo Opening Release folder...
explorer Release

cd ..
pause
