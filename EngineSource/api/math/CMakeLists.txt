# Eyelib Math API (header-only, forwards to eyelib/math)

add_library(EyelibMathAPI INTERFACE)

target_include_directories(EyelibMathAPI
    INTERFACE
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibMathAPI
    INTERFACE
        EyelibMath
)
