cmake_minimum_required(VERSION 3.20)
project(OpenEye VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/MP)
    add_compile_options(/wd4245 /wd4100 /wd4189 /wd4244 /wd4034 /wd4127)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Build configuration
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

# Output directories: engine runtime layout (bin/lib under Engine/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Engine/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Engine/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Engine/lib)

# Options
option(OPENEYE_BUILD_EDITOR "Build development editor" ON)
option(OPENEYE_BUILD_GAME "Build final game" ON)
option(OPENEYE_BUILD_PLAYER "Build unified runtime player" ON)
option(OPENEYE_BUILD_TESTS "Build API test app" ON)
option(OPENEYE_BUILD_STREAMING "Build VR streaming server (UDP discovery + frame stream)" ON)
option(OPENEYE_STREAMING_H264 "Use H.264 encoding for VR streaming (Tienet MF/NVENC)" ON)
option(OPENEYE_STREAMING_NVENC "Use NVENC for H.264 when available (requires NVIDIA Video Codec SDK)" OFF)
option(OPENEYE_USE_LZ4 "Use LZ4 for asset/binary compression" ON)
option(OPENEYE_USE_ZSTD "Use Zstd for asset/binary compression" ON)
option(OPENEYE_USE_DILIGENT "Use Diligent Engine as RHI backend (requires thirdparty/DiligentCore)" OFF)
option(OPENEYE_USE_OPENXR "Build OpenXR VR provider (requires thirdparty/OpenXR-SDK)" OFF)

# Player build uses OpenGL only (no Diligent)
if(OPENEYE_BUILD_PLAYER)
    if(OPENEYE_USE_DILIGENT)
        set(OPENEYE_USE_DILIGENT OFF CACHE BOOL "Use Diligent (disabled for player; OpenGL is default)" FORCE)
        message(STATUS "Player build: Diligent disabled; OpenGL is the default backend")
    endif()
    set(OPENEYE_DILIGENT_AVAILABLE 0 CACHE INTERNAL "Diligent disabled for player build" FORCE)
endif()

# Engine source (all thirdparty, eyelib, api, app + asset_pipe live here)
add_subdirectory(EngineSource)
