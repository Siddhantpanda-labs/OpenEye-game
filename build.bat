@echo off
REM Simple build script - compiles everything (libraries and client apps)

setlocal

REM Set build configuration (Debug or Release)
set BUILD_CONFIG=Debug
if "%1"=="Release" set BUILD_CONFIG=Release
if "%1"=="release" set BUILD_CONFIG=Release

REM Create build directory if needed
if not exist build mkdir build
cd build

REM Always (re)configure CMake so include-dir / file list changes propagate.
REM This is fast if nothing changed, and avoids stale build issues.
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF -DOPENEYE_BUILD_TESTS=ON
if errorlevel 1 (
    cd ..
    exit /b 1
)

REM Build the API test app (pulls in required libs)
REM Avoid building INTERFACE targets directly (no vcxproj).
cmake --build . --config %BUILD_CONFIG% --parallel --target OpenEyeApiTests
if errorlevel 1 (
    cd ..
    exit /b 1
)

cd ..
echo Build complete: build\bin\%BUILD_CONFIG%\
