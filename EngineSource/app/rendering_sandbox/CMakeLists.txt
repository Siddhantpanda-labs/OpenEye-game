# Sandbox executable for the next-gen rendering module (EyelibRendering only).

add_executable(OpenEyeRenderingSandbox
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(OpenEyeRenderingSandbox
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
)

target_link_libraries(OpenEyeRenderingSandbox
    PRIVATE
        EyelibRendering
        glfw
)

if(MSVC)
    target_compile_options(OpenEyeRenderingSandbox PRIVATE /W4 /wd4100)
endif()
