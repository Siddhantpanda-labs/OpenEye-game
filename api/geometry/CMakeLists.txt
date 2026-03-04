# Eyelib Geometry API

set(API_GEOMETRY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Selection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Selection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Mask.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mask.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshOps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshOps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Primitives.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Primitives.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibGeometryAPI STATIC ${API_GEOMETRY_SOURCES})

target_include_directories(EyelibGeometryAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/api
)

target_link_libraries(EyelibGeometryAPI
    PUBLIC
        EyelibAssets
        EyelibGeometry
        EyelibRenderAPI
        EyelibMath
)
