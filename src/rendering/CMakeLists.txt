# OpenEye Rendering Component
# Rendering systems, resources, passes, framegraph

set(RENDERING_RESOURCES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Material.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/MeshLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/MeshLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/UniformBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/UniformBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Cubemap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Cubemap.h
)

set(RENDERING_SYSTEMS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Frustum.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Frustum.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LODManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LODManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/InstancedRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/InstancedRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SkyboxRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SkyboxRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IBL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IBL.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShadowMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.h
)

set(RENDERING_PASSES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.h
)

set(RENDERING_FRAMEGRAPH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.h
)

add_library(OpenEyeRendering STATIC
    ${RENDERING_RESOURCES_SOURCES}
    ${RENDERING_SYSTEMS_SOURCES}
    ${RENDERING_PASSES_SOURCES}
    ${RENDERING_FRAMEGRAPH_SOURCES}
)

target_include_directories(OpenEyeRendering
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        # API folder for apps to use
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include/rendering/api>
    PRIVATE
        # Third-party includes - internal only, not exposed to apps
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
        ${CMAKE_SOURCE_DIR}/thirdparty/stb
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
)

# Dependencies
target_link_libraries(OpenEyeRendering
    PUBLIC
        OpenEyeCore
        glad
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib
)

# Copy Assimp DLL to output directory on Windows
if(WIN32)
    add_custom_command(TARGET OpenEyeRendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/bin/assimp-vc143-mt.dll
        $<TARGET_FILE_DIR:OpenEyeRendering>
    )
endif()

