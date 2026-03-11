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
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/api
)

target_link_libraries(EyelibGeometryAPI
    PUBLIC
        EyelibGeometry
        EyelibRenderAPI
        EyelibMath
)
