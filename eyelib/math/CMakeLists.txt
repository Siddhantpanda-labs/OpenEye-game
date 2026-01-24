# Eyelib Math Component (GLM-backed)

add_library(EyelibMath INTERFACE)

target_include_directories(EyelibMath
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glm>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibMath INTERFACE)

# Phase 1 verification: compile all math headers (math-only build target)
add_library(EyelibMathVerify STATIC MathVerify.cpp)
target_link_libraries(EyelibMathVerify PRIVATE EyelibMath)
target_include_directories(EyelibMathVerify PRIVATE ${CMAKE_SOURCE_DIR})

