# Eyelib Math Component (GLM-backed)

add_library(EyelibMath INTERFACE)

target_include_directories(EyelibMath
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glm>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibMath
    INTERFACE
        # GLM is header-only, but keep include path explicit
)

