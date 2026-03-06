# VR module: XR abstraction + null provider (no OpenXR here).
# OpenXR and other backends implement IXRProvider in separate targets.

add_library(EyelibVR STATIC
    NullXRProvider.cpp
)
target_include_directories(EyelibVR PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib/math>
)
target_link_libraries(EyelibVR PUBLIC EyelibMath)

if(OPENEYE_USE_OPENXR)
    add_subdirectory(openxr)
endif()
