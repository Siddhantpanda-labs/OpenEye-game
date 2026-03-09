# Eyelib Render Component (minimal bring-up)

set(RENDER_SOURCES
    # RHI (graphics backend abstraction)
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/RHIUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/GLEnums.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/BindlessHeapGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/BufferGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/TextureGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/ShaderGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/PipelineGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/RenderTargetGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/QueryPoolGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/CommandListGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/DeferredCommandListGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/DescriptorSetGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/DeviceGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/CreateDevice.cpp
)

# Diligent Engine RHI backend (only built when Diligent is available; see documentation/plans/RHI_Diligent_Integration_Plan.md)
if(OPENEYE_DILIGENT_AVAILABLE)
list(APPEND RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/DiligentEnums.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/BufferDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/TextureDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/ShaderDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/RenderTargetDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/QueryPoolDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/DescriptorSetDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/PipelineDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/CommandListDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/DeviceDiligent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/diligent/CreateDeviceDiligent.cpp
)
endif()

list(APPEND RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FullscreenQuad.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GpuProfilerRHI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GpuProfilerRHI.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraphRecipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderSourceLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderReflection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderReflection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingHooks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingHooks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GpuCullPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GpuCullPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightCullPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightCullPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshDescriptorUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshDescriptorUtils.h
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
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glad/include
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glm
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/stb
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/include
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

if(OPENEYE_DILIGENT_AVAILABLE)
    target_compile_definitions(EyelibRender PRIVATE OPENEYE_USE_DILIGENT=1)
    if(WIN32)
        target_compile_definitions(EyelibRender PRIVATE PLATFORM_WIN32=1)
    endif()
    target_include_directories(EyelibRender PRIVATE ${OPENEYE_DILIGENT_SOURCE_DIR})
    # Link static engine backends. Target names from DiligentCore: OpenGL, D3D11, Vk (not "Vulkan").
    set(_diligent_libs
        Diligent-GraphicsEngineOpenGL-static
    )
    if(WIN32)
        list(APPEND _diligent_libs Diligent-GraphicsEngineD3D11-static)
    endif()
    if(TARGET Diligent-GraphicsEngineVk-static)
        list(APPEND _diligent_libs Diligent-GraphicsEngineVk-static)
    else()
        message(WARNING "Diligent Vulkan target Diligent-GraphicsEngineVk-static not found; link without Vk. Install Vulkan SDK and reconfigure if you need Vulkan.")
    endif()
    target_link_libraries(EyelibRender PRIVATE ${_diligent_libs})
endif()

if(WIN32)
    target_link_libraries(EyelibRender PRIVATE opengl32 ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
endif()

