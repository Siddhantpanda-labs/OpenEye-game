#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

class GBuffer;
class LightingPass;
class CascadeShadowMap;
class IBL;

/** Inputs for deferred lighting: scene data + RHI objects the pipeline owns (frame ring, acquired pass set). */
struct DeferredLightingPassBuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    rhi::ClearValue clearColor = { 0.f, 0.f, 0.f, 1.f };
    LightParams lightParams{};
    CascadeShadowMap* cascadeForLighting = nullptr;
    IBL* ibl = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    rhi::IBuffer* frameUbo = nullptr;
    math::Mat4 prevView = math::Mat4(1.0f);
    math::Mat4 prevProjection = math::Mat4(1.0f);
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    /** G-buffer textures bound into the deferred lighting descriptor set (build stage). */
    GBuffer* gbuffer = nullptr;
    rhi::IDescriptorSet* frameSet = nullptr;
    /** Per-frame deferred lighting pass set from pipeline resource builder (AcquireDeferredLightingSet). */
    rhi::IDescriptorSet* deferredLightingSet = nullptr;
    LightingPass* lightingPass = nullptr;
};

struct DeferredLightingPassExecuteParams {
    DeferredLightingPassBuildParams build{};
};

class DeferredLightingPassSystem {
public:
    /** Fills deferred lighting descriptor set and LightingPass::SetResourceSpaceSets; no draws (build-only). */
    DeferredLightingPassExecuteParams BuildParams(const DeferredLightingPassBuildParams& params) const;
    bool Execute(const DeferredLightingPassExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

