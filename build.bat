@echo off
REM OpenEye Engine Build Script
REM Builds all library files, third-party files, and client applications

setlocal enabledelayedexpansion

echo ========================================
echo OpenEye Engine Build Script
echo ========================================
echo.

REM Set build configuration (Debug or Release)
set BUILD_CONFIG=Debug
if "%1"=="Release" (
    set BUILD_CONFIG=Release
) else if "%1"=="release" (
    set BUILD_CONFIG=Release
)

REM Check if CMake is available
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake is not found in PATH!
    echo Please install CMake 3.20 or later and add it to your PATH.
    pause
    exit /b 1
)

REM Get CMake version
for /f "tokens=3" %%i in ('cmake --version ^| findstr /C:"version"') do set CMAKE_VERSION=%%i
echo [INFO] CMake version: %CMAKE_VERSION%
echo [INFO] Build configuration: %BUILD_CONFIG%
echo.

REM Set build directory
set BUILD_DIR=build
if not exist %BUILD_DIR% (
    echo [INFO] Creating build directory: %BUILD_DIR%
    mkdir %BUILD_DIR%
)

REM Change to build directory
cd %BUILD_DIR%

REM Configure CMake if needed
if not exist CMakeCache.txt (
    echo [INFO] Configuring CMake...
    echo.
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] CMake configuration failed!
        cd ..
        pause
        exit /b 1
    )
    echo [SUCCESS] CMake configuration complete!
    echo.
) else (
    echo [INFO] CMake already configured. Skipping configuration...
    echo [INFO] To reconfigure, delete CMakeCache.txt or run: cmake .. -G "Visual Studio 17 2022" -A x64
    echo.
)

REM Build everything
echo ========================================
echo Building OpenEye Engine...
echo ========================================
echo.
echo [INFO] Building third-party libraries (glad, glfw, bullet3)...
echo [INFO] Building library components (core, rendering, physics, ecs, etc.)...
echo [INFO] Building client applications (OpenMeta)...
echo.

cmake --build . --config %BUILD_CONFIG% --parallel
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Output location: %CD%\bin\%BUILD_CONFIG%\
echo.
echo Executables:
if exist bin\%BUILD_CONFIG%\OpenMeta.exe (
    echo   - OpenMeta.exe (Game Application)
)
if exist bin\%BUILD_CONFIG%\Editor.exe (
    echo   - Editor.exe (Editor Application)
)
echo.
echo Libraries: %CD%\lib\%BUILD_CONFIG%\
echo.

REM Go back to project root
cd ..

echo [SUCCESS] All build targets completed successfully!
echo.
pause
