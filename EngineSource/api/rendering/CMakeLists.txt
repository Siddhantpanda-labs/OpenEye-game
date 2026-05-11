# EyelibRenderingAPI — public PIMPL facade for the next-gen rendering module (eyelib/rendering).
#
# Discipline (mandatory):
#   1. Public headers under api/rendering/ expose ONLY opaque PIMPL types and POD types.
#      No eyelib::rendering::* and no eyelib::rendering::rhi::* in public headers.
#   2. Implementation .cpp files in this tree are the ONLY place outside
#      EngineSource/eyelib/rendering/ that may #include "eyelib/rendering/..." headers.
#   3. This library MUST NOT include legacy api/render/ or eyelib/render/ headers
#      (enforced by LegacyRenderFirewall.cmake below).
#   4. Namespace: eyelib::api::rendering (distinct from legacy eyelib::api).

set(API_RENDERING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderDevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CameraView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderTarget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.cpp
    # M4.0a — public POD light descriptors (Directional / Point / Spot). Mirror
    # eyelib::rendering::SceneLighting state 1:1 in field shape so RenderPipeline::Render
    # can do a structural copy + clear-and-readd into the new module's process-global
    # lighting bus each frame.
    ${CMAKE_CURRENT_SOURCE_DIR}/Light.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderScene.h
    # M3.6 — public RAII wrapper around the new module's transient command-list slice
    # contract. Lives here (not in app/editor) because editor TUs are forbidden from
    # including eyelib/rendering/rhi/* directly per the LegacyRenderFirewall + Hard
    # constraint. Shape mirrors openeye::editor::VulkanRenderSliceScope.
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderSliceScope.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderSliceScope.cpp
    # M4.7 — public POD mirror of `eyelib::rendering::rhi::ImGuiVulkanInitInfo` so the
    # editor `ScopedImGui` and player `RuntimeOverlay` can bring up Dear ImGui-Vulkan
    # without including the internal RHI namespace. Implementation is in RenderPipeline.cpp
    # (delegates to IDevice::GetImGuiVulkanInitInfo).
    ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiVulkanInitInfo.h
    # M3.7 — public ResourceCache façade. Vends api/rendering Mesh / Material /
    # Texture2D by AssetId. M3.7b (this milestone) lights up the real loader bodies:
    # CreateMesh / CreateTexture / CreateMaterial drive eyelib::rendering::Mesh /
    # Texture2D / MaterialTable directly. The cache also implements
    # IMeshHandleResolver and registers itself as the mesh resolver + material publisher
    # on every attached RenderPipeline. Public POD inputs live alongside (MeshData.h,
    # TextureData.h, MaterialRecord.h) — these are the renderer-agnostic descriptors
    # that editor / player author code populates.
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TextureData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MaterialRecord.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceCache.cpp
    # M1.6 GPU resource handle facades (PIMPL; construction via internal/ResourceAccess.h).
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MaterialInstance.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture2D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TextureCube.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture3D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TextureArray.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Textures.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Shader.cpp
    # Internal seam — friend-only construction helpers. NEVER include from outside this lib.
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/ResourceAccess.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderingApi.cpp
    # M1.9 compile-sanity TU — proves headers compose + PIMPL boundary holds + friend
    # access seam compiles. Body never executes; static_asserts + compilation are the test.
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderingApiSanity.cpp
)

add_library(EyelibRenderingAPI STATIC ${API_RENDERING_SOURCES})

target_include_directories(EyelibRenderingAPI
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
)

target_link_libraries(EyelibRenderingAPI
    PUBLIC
        EyelibCoreAPI
        EyelibMath
    PRIVATE
        EyelibPlatformAPI
        EyelibRendering
)

# M0.6 firewall: api/rendering is the public face of the new module. It MUST NOT include
# legacy api/render/ or eyelib/render/ headers (it may include eyelib/rendering/ from .cpp
# only — see Discipline rule 2 above). Active from day one because this directory is new
# and was never on the legacy stack.
include(${OPENEYE_ENGINE_SOURCE}/cmake/LegacyRenderFirewall.cmake)
openeye_enforce_no_legacy_render_includes(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    LABEL     "EyelibRenderingAPI"
)
