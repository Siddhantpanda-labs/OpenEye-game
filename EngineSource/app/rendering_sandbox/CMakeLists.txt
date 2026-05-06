# Sandbox executable for the next-gen rendering module (EyelibRendering only).

add_executable(OpenEyeRenderingSandbox
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(OpenEyeRenderingSandbox
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/include
)

target_link_libraries(OpenEyeRenderingSandbox
    PRIVATE
        EyelibRendering
        glfw
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/lib/assimp-vc143-mt.lib
)

add_dependencies(OpenEyeRenderingSandbox CopyEngineAssets)

if(MSVC)
    target_compile_options(OpenEyeRenderingSandbox PRIVATE /W4 /wd4100)
endif()

# Second entry: rotating cube + checker.png (no Assimp).
add_executable(OpenEyeRenderingSandboxChecker
    ${CMAKE_CURRENT_SOURCE_DIR}/main_textured_cube.cpp
)
target_include_directories(OpenEyeRenderingSandboxChecker
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
)
target_link_libraries(OpenEyeRenderingSandboxChecker
    PRIVATE
        EyelibRendering
        glfw
)
add_dependencies(OpenEyeRenderingSandboxChecker CopyEngineAssets)
if(MSVC)
    target_compile_options(OpenEyeRenderingSandboxChecker PRIVATE /W4 /wd4100)
endif()
