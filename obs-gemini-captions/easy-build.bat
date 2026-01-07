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

REM Create build directory
if not exist "build" (
    echo [INFO] Creating build directory...
    mkdir build
)

cd build

echo.
echo [INFO] Configuring Project (Visual Studio 2022 x64)...
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo [ERROR] Configuration failed.
    echo Did you run 'setup-sdk.ps1' first?
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
