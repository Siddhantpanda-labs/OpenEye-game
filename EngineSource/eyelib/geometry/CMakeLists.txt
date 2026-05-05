# Eyelib Geometry Module

set(GEOMETRY_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/MeshSnapshot.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshExtrude.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshExtrude.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshBevel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshBevel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshBoolean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshBoolean.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDecimate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDecimate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDeform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshDeform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshTexturePaint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshTexturePaint.h
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshColorPaint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operations/MeshColorPaint.h
)

set(GEOMETRY_FIELDS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/NoiseField.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/NoiseField.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/Field.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fields/Field.h
)

set(GEOMETRY_MESH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/EditableMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditableMesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshBinaryFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshBinaryFormat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/OEAFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/OEAFormat.h
)

set(GEOMETRY_SOURCES
    ${GEOMETRY_CORE_SOURCES}
    ${GEOMETRY_SELECTION_SOURCES}
    ${GEOMETRY_MASKING_SOURCES}
    ${GEOMETRY_PRIMITIVES_SOURCES}
    ${GEOMETRY_OPERATIONS_SOURCES}
    ${GEOMETRY_FIELDS_SOURCES}
    ${GEOMETRY_MESH_SOURCES}
)

add_library(EyelibGeometry STATIC ${GEOMETRY_SOURCES})

target_include_directories(EyelibGeometry
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
)

target_link_libraries(EyelibGeometry
    PUBLIC
        AssetCore
        EyelibRender
        EyelibMath
        EyelibCore
        EyelibPlatform
    PRIVATE
)
