# Windows VR streaming test client (same protocol as Flutter client).
# Receives frames, decodes (H.264/JPEG), displays in GLFW window; mouse drag = head rotation (GYRO).
cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
  message(STATUS "win_client: skipped (Windows only)")
  return()
endif()

set(WIN_CLIENT_SOURCES
  main.cpp
  VRStreamClient.cpp
  FrameDecoder.cpp
)

add_executable(WinClient ${WIN_CLIENT_SOURCES})

target_include_directories(WinClient PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENEYE_ENGINE_SOURCE}/thirdparty/glad/include
  ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
  ${OPENEYE_ENGINE_SOURCE}/thirdparty/stb
)

target_link_libraries(WinClient PRIVATE
  glfw
  glad
  ws2_32
)

# Media Foundation for H.264 decode
target_link_libraries(WinClient PRIVATE
  mfplat
  mf
  mfreadwrite
  mfuuid
)

target_compile_definitions(WinClient PRIVATE
  _CRT_SECURE_NO_WARNINGS
  WIN32_LEAN_AND_MEAN
)

if(MSVC)
  target_compile_options(WinClient PRIVATE /W3)
endif()

# STB_IMAGE_IMPLEMENTATION defined in FrameDecoder.cpp only
