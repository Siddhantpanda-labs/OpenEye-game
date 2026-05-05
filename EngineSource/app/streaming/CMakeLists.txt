# VR streaming server (optional): UDP discovery + frame stream + gyro input

set(STREAMING_SOURCES
    VRStreamingServer.cpp
)

add_library(OpenEyeStreaming STATIC ${STREAMING_SOURCES})

target_include_directories(OpenEyeStreaming
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OPENEYE_ENGINE_SOURCE}
)

# Tienet provides StreamServer (network/protocol); H264 uses Tienet encoder when enabled
target_link_libraries(OpenEyeStreaming
    PUBLIC
        Tienet
)

if(OPENEYE_STREAMING_H264)
    target_compile_definitions(OpenEyeStreaming PRIVATE OPENEYE_STREAMING_H264=1)
endif()
