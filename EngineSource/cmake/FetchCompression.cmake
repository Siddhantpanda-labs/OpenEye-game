# FetchContent setup for LZ4 and Zstd. OPENEYE_USE_LZ4 / OPENEYE_USE_ZSTD defined in root.
include(FetchContent)

# -----------------------------------------------------------------------------
# LZ4 (project lives in build/cmake)
# -----------------------------------------------------------------------------
if(OPENEYE_USE_LZ4)
    set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(lz4
        GIT_REPOSITORY https://github.com/lz4/lz4.git
        GIT_TAG        v1.9.4
        SOURCE_SUBDIR  build/cmake
    )
    FetchContent_MakeAvailable(lz4)
    if(TARGET lz4_static)
        add_library(lz4 ALIAS lz4_static)
    elseif(TARGET lz4_shared)
        add_library(lz4 ALIAS lz4_shared)
    endif()
    set(OPENEYE_LZ4_AVAILABLE 1)
else()
    set(OPENEYE_LZ4_AVAILABLE 0)
endif()

# -----------------------------------------------------------------------------
# Zstd
# -----------------------------------------------------------------------------
if(OPENEYE_USE_ZSTD)
    FetchContent_Declare(zstd
        URL      https://github.com/facebook/zstd/releases/download/v1.5.5/zstd-1.5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR build/cmake
    )
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zstd)
    set(OPENEYE_ZSTD_AVAILABLE 1)
else()
    set(OPENEYE_ZSTD_AVAILABLE 0)
endif()
