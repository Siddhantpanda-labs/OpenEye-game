# OpenEye Assets Component
# Asset management: GUID, Registry, Cooker

set(ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetGUID.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetGUID.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCooker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCooker.h
)

add_library(OpenEyeAssets STATIC ${ASSETS_SOURCES})

target_include_directories(OpenEyeAssets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

# Dependencies
target_link_libraries(OpenEyeAssets
    PUBLIC
        OpenEyeCore
)

