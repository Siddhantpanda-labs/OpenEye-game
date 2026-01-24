# Eyelib Render Component (minimal bring-up)

set(RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShadowMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ClusteredLightGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClusteredLightGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture2D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture3D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureCube.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureArray.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/MaterialInstance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/MeshLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/MeshOptimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/InstancedRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BatchRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Sprite.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Sprite.h
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/SpriteRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/SpriteRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Font.h
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/TextRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/TextRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Primitives.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/2d/Primitives.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LODManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LODManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CullingSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CullingSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IBL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IBL.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderTarget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderTarget.h
)

add_library(EyelibRender STATIC ${RENDER_SOURCES})

target_include_directories(EyelibRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
        ${CMAKE_SOURCE_DIR}/thirdparty/stb
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
)

target_link_libraries(EyelibRender
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibPlatform
    PRIVATE
        glad
)

if(WIN32)
    target_link_libraries(EyelibRender PRIVATE opengl32 ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
endif()

