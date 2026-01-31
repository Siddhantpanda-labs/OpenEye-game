# Tienet - Generic Networking Library
cmake_minimum_required(VERSION 3.16)

project(Tienet VERSION 1.0.0 LANGUAGES CXX)
# Feature toggles
option(TIENET_ENABLE_OPENSSL "Enable OpenSSL for crypto" OFF)
option(TIENET_ENABLE_SQLITE "Enable SQLite3 for DB persistence" OFF)
option(TIENET_ENABLE_NVENC "Enable NVIDIA NVENC encoder" OFF)
option(TIENET_ENABLE_ANDROID_MEDIACODEC "Enable Android MediaCodec decoder" OFF)
option(TIENET_ENABLE_CUDA "Enable CUDA interop (for Vulkan→NVENC)" OFF)


# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Core library sources
set(TIENET_CORE_SOURCES
    src/tienet.cpp
    src/core/socket.cpp
    src/core/packet.cpp
    src/core/protocol.cpp
    src/core/rate_control.cpp
    src/core/pacer.cpp
    src/core/fec.cpp
    src/core/session_manager.cpp
    src/core/channels.cpp
    src/core/advanced_compression.cpp
    src/core/congestion_control.cpp
    src/core/priority_scheduler.cpp
    src/core/channel_persistence.cpp
    src/core/security.cpp
    src/core/db_persistence.cpp
    src/core/channel_pool.cpp
    src/core/metrics.cpp
)

# Server sources
set(TIENET_SERVER_SOURCES
    src/server/server.cpp
)

# Client sources
set(TIENET_CLIENT_SOURCES
    src/client/client.cpp
)

# Compression sources
set(TIENET_COMPRESSION_SOURCES
    src/compression/compressor.cpp
)

# Media sources
set(TIENET_MEDIA_SOURCES
    src/media/encoder.cpp
    src/media/decoder.cpp
)

# VR extension sources
set(TIENET_VR_SOURCES
    src/vr/vr_server.cpp
)

# Flutter extension sources
set(TIENET_FLUTTER_SOURCES
    src/flutter/flutter_bindings.cpp
)

# All sources
set(TIENET_ALL_SOURCES
    ${TIENET_CORE_SOURCES}
    ${TIENET_SERVER_SOURCES}
    ${TIENET_CLIENT_SOURCES}
    ${TIENET_COMPRESSION_SOURCES}
    ${TIENET_MEDIA_SOURCES}
    ${TIENET_VR_SOURCES}
    ${TIENET_FLUTTER_SOURCES}
    src/discovery/discovery.cpp
)

# Create the main library
add_library(Tienet STATIC ${TIENET_ALL_SOURCES})

# Set target properties
set_target_properties(Tienet PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PUBLIC_HEADER include/tienet/core/socket.hpp
)

# Include directories for the library
target_include_directories(Tienet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(Tienet PRIVATE ws2_32 mfplat mfuuid mf ole32)
elseif(UNIX)
    target_link_libraries(Tienet PRIVATE pthread)
endif()

# Optional: Add compression libraries
# Avoid pkg-config/MSYS headers when using MSVC to prevent header conflicts
if(NOT MSVC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # LZ4 compression
        pkg_check_modules(LZ4 QUIET liblz4)
        if(LZ4_FOUND)
            target_link_libraries(Tienet PRIVATE ${LZ4_LIBRARIES})
            target_include_directories(Tienet PRIVATE ${LZ4_INCLUDE_DIRS})
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_LZ4)
        endif()
        
        # Zstd compression
        pkg_check_modules(ZSTD QUIET libzstd)
        if(ZSTD_FOUND)
            target_link_libraries(Tienet PRIVATE ${ZSTD_LIBRARIES})
            target_include_directories(Tienet PRIVATE ${ZSTD_INCLUDE_DIRS})
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_ZSTD)
        endif()
    endif()
endif()

# Windows-specific compression libraries
if(WIN32)
    # OpenSSL (for AES-GCM). Only enable when explicitly requested to avoid MSYS header conflicts
    if(TIENET_ENABLE_OPENSSL)
        find_package(OpenSSL QUIET)
        if(OPENSSL_FOUND)
            target_link_libraries(Tienet PRIVATE OpenSSL::SSL OpenSSL::Crypto)
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_OPENSSL)
        endif()
    endif()
    
    # SQLite3 (optional). Only enable when explicitly requested
    if(TIENET_ENABLE_SQLITE)
        find_package(SQLite3 QUIET)
        if(SQLite3_FOUND)
            target_link_libraries(Tienet PRIVATE SQLite::SQLite3)
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_SQLITE3)
        endif()
    endif()
    # NVENC (optional): Requires NVIDIA Video Codec SDK
    if(TIENET_ENABLE_NVENC)
        set(_NVENC_HINTS
            $ENV{VIDEO_CODEC_SDK_DIR}
            $ENV{NVENC_SDK_DIR}
            "C:/Program Files/NVIDIA GPU Computing Toolkit/Video Codec SDK"
        )
        find_path(NVENC_INCLUDE_DIR nvEncodeAPI.h HINTS ${_NVENC_HINTS} PATH_SUFFIXES Interface interface include Include)
        find_library(NVENC_LIBRARY NAMES nvencodeapi nvEncodeAPI64 nvencodeapi64 HINTS ${_NVENC_HINTS} PATH_SUFFIXES lib Lib lib/x64 Lib/Win64)
        if(NVENC_INCLUDE_DIR AND NVENC_LIBRARY)
            target_include_directories(Tienet PRIVATE ${NVENC_INCLUDE_DIR})
            target_link_libraries(Tienet PRIVATE ${NVENC_LIBRARY})
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_NVENC)
            message(STATUS "NVENC enabled: ${NVENC_INCLUDE_DIR}")
        else()
            message(WARNING "NVENC requested but nvEncodeAPI.h or library not found. Set VIDEO_CODEC_SDK_DIR and reconfigure.")
        endif()
    endif()
    # CUDA driver for interop
    if(TIENET_ENABLE_CUDA)
        find_library(NVCUDA_LIBRARY nvcuda)
        if(NVCUDA_LIBRARY)
            target_link_libraries(Tienet PRIVATE ${NVCUDA_LIBRARY})
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_CUDA)
            message(STATUS "CUDA driver enabled for interop")
        else()
            message(WARNING "CUDA requested but nvcuda.lib not found (ensure NVIDIA driver SDK available)")
        endif()
    endif()
    # Try to find LZ4 via vcpkg or manual installation
    find_path(LZ4_INCLUDE_DIR lz4.h)
    find_library(LZ4_LIBRARY lz4)
    if(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
        target_include_directories(Tienet PRIVATE ${LZ4_INCLUDE_DIR})
        target_link_libraries(Tienet PRIVATE ${LZ4_LIBRARY})
        target_compile_definitions(Tienet PRIVATE TIENET_HAS_LZ4)
    endif()
    
    # Try to find Zstd via vcpkg or manual installation
    find_path(ZSTD_INCLUDE_DIR zstd.h)
    find_library(ZSTD_LIBRARY zstd)
    if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
        target_include_directories(Tienet PRIVATE ${ZSTD_INCLUDE_DIR})
        target_link_libraries(Tienet PRIVATE ${ZSTD_LIBRARY})
        target_compile_definitions(Tienet PRIVATE TIENET_HAS_ZSTD)
    endif()
endif()

# Android-specific linkage
if(ANDROID)
    if(TIENET_ENABLE_ANDROID_MEDIACODEC)
        find_library(MEDIANDK_LIBRARY mediandk)
        find_library(ANDROID_LOG_LIB log)
        if(MEDIANDK_LIBRARY)
            target_link_libraries(Tienet PRIVATE ${MEDIANDK_LIBRARY})
            if(ANDROID_LOG_LIB)
                target_link_libraries(Tienet PRIVATE ${ANDROID_LOG_LIB})
            endif()
            target_compile_definitions(Tienet PRIVATE TIENET_HAS_MEDIACODEC)
            message(STATUS "Android MediaCodec enabled")
        else()
            message(WARNING "MediaCodec requested but mediandk not found. Ensure NDK is installed.")
        endif()
    endif()
endif()

# Create examples
add_subdirectory(examples)

# Create tests
add_subdirectory(tests)

# Installation
install(TARGETS Tienet
    EXPORT TienetTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tienet
)

install(DIRECTORY include/tienet/
    DESTINATION include/tienet
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT TienetTargets
    FILE TienetTargets.cmake
    NAMESPACE Tienet::
    DESTINATION lib/cmake/Tienet
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    TienetConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TienetConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TienetConfig.cmake
    INSTALL_DESTINATION lib/cmake/Tienet
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TienetConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/TienetConfigVersion.cmake
    DESTINATION lib/cmake/Tienet
)

# Print configuration info
message(STATUS "Tienet ${PROJECT_VERSION} configured successfully")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
