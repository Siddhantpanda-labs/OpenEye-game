# VR streaming server (optional): UDP discovery + frame stream + gyro input

set(STREAMING_SOURCES
    VRStreamingServer.cpp
    FrameEncoder.cpp
)

add_library(OpenEyeStreaming STATIC ${STREAMING_SOURCES})

target_include_directories(OpenEyeStreaming
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(OpenEyeStreaming
    PUBLIC
        # stb_image_write is header-only; no link
)
