# Eyelib Math Component (GLM-backed)

add_library(EyelibMath INTERFACE)

target_sources(EyelibMath INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Transform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AABB.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Sphere.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Capsule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Frustum.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Plane.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Intersection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TransformUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interpolation.h
)

target_include_directories(EyelibMath
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glm>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibMath INTERFACE)

# Phase 1 verification: compile all math headers (math-only build target)
if(OPENEYE_BUILD_TESTS)
    add_library(EyelibMathVerify STATIC MathVerify.cpp)
    target_link_libraries(EyelibMathVerify PRIVATE EyelibMath)
    target_include_directories(EyelibMathVerify PRIVATE ${CMAKE_SOURCE_DIR})
endif()

