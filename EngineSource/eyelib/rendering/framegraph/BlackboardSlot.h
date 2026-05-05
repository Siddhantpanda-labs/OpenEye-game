#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {

/** Typed blackboard slots (extend as the module grows). */
enum class BlackboardSlot : uint8_t {
    /** Main HDR color render target owned by `RenderResources`; uses `MainDepth` as its shared depth attachment. */
    MainHdrRenderTarget = 0,
    /** G-Buffer target (Albedo, Normal, Material) owned by `RenderResources`; uses `MainDepth` as its shared depth attachment. */
    GBufferRenderTarget,
    /** Standalone depth texture shared by `MainHdrRenderTarget` and any depth-only pass (e.g. depth prepass). */
    MainDepth,
    /** Per-frame uniform buffer (time, frame index, global constants). Null until created (A.5). */
    PerFrameUniformBuffer,
    /** Per-view uniform buffer (camera matrices, viewport, jitter). Null until created (A.5). */
    PerViewUniformBuffer,
    /** Per-draw instance SSBO (transforms + material ids), std430. Filled in upload phase (B.4). */
    PerObjectStorageBuffer,
    /** Material-table SSBO (`materials::MaterialGpuStd430` rows), indexed by `DrawItem::materialId`. */
    MaterialTableStorageBuffer,
    /** Single directional light UBO (std140, 256 bytes). Filled in upload phase (B.6). */
    DirectionalLightUniformBuffer,
    /** Default diffuse IBL cubemap (linear RGBA16F); published by `IblDefaultProbes` / `writer_names::kLightingIbl`. */
    IblIrradianceCubemap,
    /** Default specular prefilter cubemap (same footprint as irradiance until a mip chain is authored). */
    IblPrefilterCubemap,
    /** BRDF split-sum LUT (`RGBA16F`, RG used). */
    IblBrdfLut,
    SlotCount
};

}  // namespace rendering
}  // namespace eyelib
