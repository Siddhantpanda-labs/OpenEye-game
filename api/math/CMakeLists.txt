# Eyelib Math API (header-only, forwards to eyelib/math)

add_library(EyelibMathAPI INTERFACE)

target_include_directories(EyelibMathAPI
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibMathAPI
    INTERFACE
        EyelibMath
)
