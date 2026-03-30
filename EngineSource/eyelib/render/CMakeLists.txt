# Eyelib Render Component (minimal bring-up)

set(RENDER_SOURCES
    # RHI (graphics backend abstraction)
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/RHIUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/ResourceSetLayoutDesc.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/OpenGLResourceSpaceBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/OpenGL/DeviceGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rhi/CreateDevice.cpp
)

# Vulkan RHI backend (see documentation/vulkan/vulkan_plan.md). Built when OPENEYE_USE_VULKAN and Vulkan SDK found.
if(OPENEYE_USE_VULKAN)
    find_package(Vulkan QUIET)
endif()
if(OPENEYE_USE_VULKAN AND Vulkan_FOUND)
    list(APPEND RENDER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/DeviceVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/DeviceVKPresentation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/CreateDeviceVulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/FrameContextVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/InstanceVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/PhysicalDeviceVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/QueuesVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/core/SwapchainVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/commands/CommandBufferVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/commands/CommandListVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/commands/CommandPoolVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/resources/BufferVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/resources/TextureVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/resources/RenderTargetVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/memory/AllocatorVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/memory/UploadHeapVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/memory/DeletionQueueVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/descriptors/DescriptorAllocatorVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/descriptors/DescriptorCacheVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/descriptors/DescriptorSetVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/descriptors/BindlessHeapVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/PipelineVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/VulkanGlslToSpirv.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/ShaderVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/VulkanPipelineHash.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/VulkanBindingUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/pipelines/VulkanResourceSpaces.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/sync/FenceVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/sync/QueryPoolVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/barriers/BarrierTrackerVK.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/Vulkan/utils/DebugUtilsVK.cpp
    )
    set(OPENEYE_VULKAN_AVAILABLE 1)
else()
    if(OPENEYE_USE_VULKAN)
        message(STATUS "Vulkan not found; Vulkan RHI backend disabled. Install Vulkan SDK to enable.")
    endif()
    set(OPENEYE_VULKAN_AVAILABLE 0)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/GBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/GBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/RenderPipelineGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/RenderPipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/FrameGraphPassNames.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/FrameRenderContext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/FrameGraphRecipeBuilders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/FrameGraphRecipeBuilders.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/RenderGraphOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/RenderGraphOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/RenderGraphRecipes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/RenderGraphRecipes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/RenderPipelineMode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/graph/FrameGraphExecutorSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shadows/CascadeShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shadows/CascadeShadowMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shadows/ShadowPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shadows/ShadowPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/SceneLighting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/SceneLighting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/ClusteredLightGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/ClusteredLightGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/detail/LightCullPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/detail/LightCullPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ibl/IBL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ibl/IBL.h
    ${CMAKE_CURRENT_SOURCE_DIR}/reflection/ReflectionProbe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reflection/ReflectionProbeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post/PostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post/PostProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraphRecipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framegraph/FrameGraph.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderSourceLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderReflection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderReflection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PipelineCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/MaterialCompiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/MaterialCompiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/MaterialShader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingHooks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingHooks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GBufferPassOpaqueParams.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/detail/GpuCullPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/detail/GpuCullPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GpuCullPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/GpuCullPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightCullPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/LightCullPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/SkyboxPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/SkyboxPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/PostProcessPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/PostProcessPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/TransparencyPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/DeferredLightingPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/DeferredLightingPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardOpaquePassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardOpaquePassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardOpaqueGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardOpaqueStereoGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardTransparentGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardPlusOpaqueGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ForwardPlusFramePrep.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/ShadowPassParams.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/EditorOverlayPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/EditorOverlayPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/StreamingPassSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/StreamingPassSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RenderBackendPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RenderBackendPolicy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/submission/BatchBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/PassResourceBuilders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/PassResourceBuilders.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/common/ForwardSharedStateWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/common/ForwardSharedStateWriter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/passes/common/ForwardSharedEnvironment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshDescriptorUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshDescriptorUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture2D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/Texture3D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureCube.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureArray.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/MaterialBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/materials/MaterialInstance.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderTarget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderTarget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDrawRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDrawRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderWatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderWatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderLoader.h
)

add_library(EyelibRender STATIC ${RENDER_SOURCES})

target_include_directories(EyelibRender
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glad/include
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
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
        glfw
)

if(Vulkan_FOUND)
    target_include_directories(EyelibRender PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(EyelibRender PRIVATE Vulkan::Vulkan)
    target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_AVAILABLE=1)
    # Runtime GLSL→SPIR-V (shaderc from Vulkan SDK Lib/)
    get_filename_component(_OPENEYE_VULKAN_SDK_ROOT "${Vulkan_INCLUDE_DIRS}" DIRECTORY)
    if(WIN32)
        set(_OPENEYE_SHADERC_REL_LIB "${_OPENEYE_VULKAN_SDK_ROOT}/Lib/shaderc_combined.lib")
        set(_OPENEYE_SHADERC_DBG_LIB "${_OPENEYE_VULKAN_SDK_ROOT}/Lib/shaderc_combinedd.lib")
        if(EXISTS "${_OPENEYE_SHADERC_DBG_LIB}" AND EXISTS "${_OPENEYE_SHADERC_REL_LIB}")
            target_link_libraries(EyelibRender PRIVATE
                "$<$<CONFIG:Debug>:${_OPENEYE_SHADERC_DBG_LIB}>"
                "$<$<NOT:$<CONFIG:Debug>>:${_OPENEYE_SHADERC_REL_LIB}>")
            target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_SHADERC=1)
        elseif(EXISTS "${_OPENEYE_SHADERC_REL_LIB}")
            target_link_libraries(EyelibRender PRIVATE "${_OPENEYE_SHADERC_REL_LIB}")
            target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_SHADERC=1)
        endif()
    else()
        find_library(_OPENEYE_SHADERC_LIB shaderc_combined PATHS "${_OPENEYE_VULKAN_SDK_ROOT}/lib" NO_DEFAULT_PATH)
        if(_OPENEYE_SHADERC_LIB)
            target_link_libraries(EyelibRender PRIVATE "${_OPENEYE_SHADERC_LIB}")
            target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_SHADERC=1)
        endif()
    endif()
    if(OPENEYE_VULKAN_DEBUG_CLEAR_RED)
        target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_DEBUG_CLEAR_RED=1)
    endif()
    if(OPENEYE_VULKAN_VALIDATION)
        target_compile_definitions(EyelibRender PRIVATE OPENEYE_VULKAN_VALIDATION=1)
    endif()
endif()

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

