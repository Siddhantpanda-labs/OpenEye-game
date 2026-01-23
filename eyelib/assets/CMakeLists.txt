# Eyelib Assets Component

set(ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.h
)

add_library(EyelibAssets STATIC ${ASSETS_SOURCES})

target_include_directories(EyelibAssets
    PUBLIC
        # Allow includes like "eyelib/assets/AssetRegistry.h"
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(EyelibAssets
    PUBLIC
        EyelibCore
)

