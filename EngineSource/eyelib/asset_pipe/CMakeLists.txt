# Asset pipeline (under eyelib): canonical formats, database, runtime, importer tool
# Include base is parent (eyelib) so #include "asset_pipe/core/..." resolves
set(ASSET_PIPE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(core)
add_subdirectory(meta)
add_subdirectory(database)
add_subdirectory(runtime)
add_subdirectory(writers)
add_subdirectory(readers)
add_subdirectory(tools/importer)
