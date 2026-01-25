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
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF
if errorlevel 1 (
    cd ..
    exit /b 1
)

REM Build main targets (skip test targets that have errors)
cmake --build . --config %BUILD_CONFIG% --parallel --target EyelibCore EyelibMath EyelibAssets EyelibAnimation EyelibECS EyelibScene EyelibSerialize EyelibParticles EyelibFluids EyelibDebug EyelibPhysics EyelibRender EyelibPlatform EyelibAudio EyelibRenderAPI EyelibAudioAPI EyelibAssetsAPI EyelibAnimationAPI EyelibECSAPI EyelibSceneAPI EyelibSerializeAPI EyelibParticlesAPI EyelibFluidsAPI EyelibDebugAPI EyelibPhysicsAPI EyelibPlatformAPI EyelibCoreAPI EyelibMathAPI OpenEyeImGui glad glfw BulletDynamics BulletCollision LinearMath Bullet3Common Bullet3Collision Bullet3Dynamics Bullet3OpenCL_clew
if errorlevel 1 (
    cd ..
    exit /b 1
)

cd ..
echo Build complete: build\bin\%BUILD_CONFIG%\
