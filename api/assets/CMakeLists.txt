# Eyelib Assets API

set(API_ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibAssetsAPI STATIC ${API_ASSETS_SOURCES})

target_include_directories(EyelibAssetsAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibAssetsAPI
    PUBLIC
        EyelibAssets
)

