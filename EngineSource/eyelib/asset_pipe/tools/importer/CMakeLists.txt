# Importer tool: Assimp -> ImportScene -> canonical files
set(IMPORTER_SOURCES
    main.cpp
    SceneLoader.cpp
    SceneConverter.cpp
    TextureExtraction.cpp
)
add_executable(AssetPipeImporter ${IMPORTER_SOURCES})

# Assimp and stb: use engine thirdparty
set(ASSIMP_ROOT "${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp")
if(NOT EXISTS "${ASSIMP_ROOT}/include/assimp/Importer.hpp")
    set(ASSIMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/assimp")
endif()
set(STB_ROOT "${OPENEYE_ENGINE_SOURCE}/thirdparty/stb")
if(NOT EXISTS "${STB_ROOT}/stb_image.h")
    set(STB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/stb")
endif()
target_include_directories(AssetPipeImporter PRIVATE
    ${ASSET_PIPE_INCLUDE_DIR}
    ${ASSIMP_ROOT}/include
    ${STB_ROOT}
)
target_link_libraries(AssetPipeImporter PRIVATE AssetPipeWriters AssetPipeMeta)

if(MSVC)
    target_link_libraries(AssetPipeImporter PRIVATE
        ${ASSIMP_ROOT}/lib/assimp-vc143-mt.lib
    )
else()
    find_package(assimp QUIET)
    if(assimp_FOUND)
        target_link_libraries(AssetPipeImporter PRIVATE assimp::assimp)
    else()
        target_link_libraries(AssetPipeImporter PRIVATE assimp)
    endif()
endif()
