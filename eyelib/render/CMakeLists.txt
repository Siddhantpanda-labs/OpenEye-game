# Eyelib Render Component (minimal bring-up)

set(RENDER_SOURCES
    # RHI (graphics backend abstraction)
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/RHIContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/RHIUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/GLEnums.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/BufferGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/TextureGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/ShaderGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/PipelineGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/RenderTargetGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/QueryPoolGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/CommandListGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/DeviceGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/CreateDevice.cpp
)

if(OPENEYE_USE_BGFX AND OPENEYE_BGFX_AVAILABLE)
    list(APPEND RENDER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/BGFXEnums.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/BufferBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/TextureBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/ShaderBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/PipelineBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/RenderTargetBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/QueryPoolBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/CommandListBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/DeviceBGFX.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/bgfx/CreateDeviceBGFX.cpp
    )
endif()

list(APPEND RENDER_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDrawRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDrawRenderer.h
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
        EyelibDebug
    PRIVATE
        glad
)

if(OPENEYE_USE_BGFX AND OPENEYE_BGFX_AVAILABLE)
    target_compile_definitions(EyelibRender PRIVATE OPENEYE_USE_BGFX=1)
    target_include_directories(EyelibRender PRIVATE ${OPENEYE_BGFX_SOURCE_DIR}/bgfx/include ${OPENEYE_BGFX_SOURCE_DIR}/bx/include)
    target_link_libraries(EyelibRender PRIVATE bgfx glfw)
endif()

if(WIN32)
    target_link_libraries(EyelibRender PRIVATE opengl32 ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
endif()

