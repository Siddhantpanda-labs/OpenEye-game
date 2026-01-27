# Eyelib Geometry Module

set(GEOMETRY_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.h
)

set(GEOMETRY_SELECTION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/selection/Selection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/selection/Selection.h
)

set(GEOMETRY_MASKING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/masking/Mask.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/masking/Mask.h
)

set(GEOMETRY_PRIMITIVES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/primitives/MeshPrimitives.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/primitives/MeshPrimitives.h
)

set(GEOMETRY_OPERATIONS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshSubdivide.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshSubdivide.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshSmooth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshSmooth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDisplace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDisplace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshNoise.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshNoise.h
)

set(GEOMETRY_FIELDS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/NoiseField.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/NoiseField.h
)

set(GEOMETRY_SOURCES
    ${GEOMETRY_CORE_SOURCES}
    ${GEOMETRY_SELECTION_SOURCES}
    ${GEOMETRY_MASKING_SOURCES}
    ${GEOMETRY_PRIMITIVES_SOURCES}
    ${GEOMETRY_OPERATIONS_SOURCES}
    ${GEOMETRY_FIELDS_SOURCES}
)

add_library(EyelibGeometry STATIC ${GEOMETRY_SOURCES})

target_include_directories(EyelibGeometry
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibGeometry
    PUBLIC
        EyelibRender
        EyelibMath
    PRIVATE
)
