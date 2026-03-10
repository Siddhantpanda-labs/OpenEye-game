# Canonical viewer: loads .mesh (asset_pipe format) and displays with default material.
# Exe ends up in Engine/bin/ (same as OpenEyePlayer).
set(CANONICAL_VIEWER_SOURCES
    main.cpp
)
add_executable(CanonicalViewer ${CANONICAL_VIEWER_SOURCES})

target_include_directories(CanonicalViewer PRIVATE
    ${OPENEYE_ENGINE_SOURCE}
    ${OPENEYE_ENGINE_SOURCE}/api
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(CanonicalViewer PRIVATE
    AssetPipeReaders
    EyelibCoreAPI
    EyelibPlatformAPI
    EyelibAssetsAPI
    EyelibRenderAPI
)
add_dependencies(CanonicalViewer CopyEngineAssets)
