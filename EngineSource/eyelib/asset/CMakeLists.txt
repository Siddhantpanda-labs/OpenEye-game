# Asset pipeline (under eyelib): canonical formats, database, runtime, importer tool
# Include base is parent (eyelib) so #include "asset/core/..." resolves
set(ASSET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(core)
add_subdirectory(meta)
add_subdirectory(database)
add_subdirectory(runtime)
add_subdirectory(readers)
add_subdirectory(writers)

# CLI mesh/material importer (assimp). Off by default — editor/player builds do not compile or ship it.
# Enable when needed: cmake -DOPENEYE_BUILD_ASSET_IMPORTER=ON (copy AssetImporter next to the editor for Asset Panel "Convert to canonical").
option(OPENEYE_BUILD_ASSET_IMPORTER "Build AssetImporter tool (not required for editor or player)" OFF)
if(OPENEYE_BUILD_ASSET_IMPORTER)
    add_subdirectory(tools/importer)
endif()
