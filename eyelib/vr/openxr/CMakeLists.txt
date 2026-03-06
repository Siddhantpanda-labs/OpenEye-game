# OpenXR provider: implements IXRProvider using OpenXR SDK.
# Only built when OPENEYE_USE_OPENXR is ON (OpenXR-SDK added in root CMakeLists).

if(NOT TARGET openxr_loader)
    message(FATAL_ERROR "EyelibVROpenXR requires openxr_loader; set OPENEYE_USE_OPENXR ON and add OpenXR-SDK first.")
endif()

add_library(EyelibVROpenXR STATIC
    OpenXRProvider.cpp
)
target_include_directories(EyelibVROpenXR PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/eyelib/vr
    ${CMAKE_SOURCE_DIR}/eyelib/math
    ${CMAKE_SOURCE_DIR}/thirdparty/OpenXR-SDK/include
)
target_link_libraries(EyelibVROpenXR PUBLIC
    openxr_loader
    EyelibVR
    glad
)
