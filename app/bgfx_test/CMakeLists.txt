# Minimal BGFX Phase 0 test: init, one frame, shutdown.
# Only built when OPENEYE_USE_BGFX=ON.
if(NOT OPENEYE_BGFX_AVAILABLE)
    return()
endif()

add_executable(OpenEyeBGFXTest ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_include_directories(OpenEyeBGFXTest
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include
)

target_link_libraries(OpenEyeBGFXTest
    PRIVATE
        bgfx
        glfw
)

if(WIN32)
    target_link_libraries(OpenEyeBGFXTest PRIVATE opengl32)
endif()
