#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

class ShaderCache;
class CascadeShadowMap;
class ReflectionProbeManager;
class IBL;
namespace resources {
class PassResourceBuilder;
}

/** Bindless + resource-space sets check shared by forward opaque / transparent / forward+ opaque paths. */
bool ForwardPbrResourceBindingsReady(rhi::IDevice* device, rhi::ICommandList* cmd, rhi::IDescriptorSet* frameSet,
    rhi::IDescriptorSet* passSet, rhi::IDescriptorSet* materialSet, rhi::IDescriptorSet* sceneBuffersSet,
    rhi::IBuffer* frameUbo, rhi::IBindlessHeap* bindlessHeap);

/** Per-frame state and acquire hooks for forward opaque draws; filled by RenderPipeline (or tests). */
struct ForwardOpaqueRenderContext {
    rhi::IDevice* device = nullptr;
    ShaderCache* shaderCache = nullptr;
    resources::PassResourceBuilder* passResourceBuilder = nullptr;
    rhi::IBuffer* frameUbo = nullptr;
    rhi::IBuffer* bonesBuffer = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    rhi::IBuffer* lightIndexBuffer = nullptr;
    rhi::IBindlessHeap* bindlessHeap = nullptr;
    rhi::IDescriptorSet* frameSet = nullptr;
    rhi::IDescriptorSet* materialSet = nullptr;
    CascadeShadowMap* cascadeShadowMap = nullptr;
    ReflectionProbeManager* reflectionProbeManager = nullptr;
    std::function<void()> ensureResourceSets;
    std::function<rhi::IDescriptorSet*()> acquirePassDrawSet;
    std::function<rhi::IDescriptorSet*()> acquireMaterialDrawSet;
    std::function<rhi::IDescriptorSet*()> acquireSceneDrawSet;
    std::function<IBL*(const RenderFrameData&)> ensureIbl;
    std::function<void(const RenderFrameData&, const CameraView&, rhi::IDescriptorSet*, rhi::IDescriptorSet*, bool)> buildForwardPassSharedState;
};

void ForwardOpaqueRenderForCamera(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
    const std::vector<Drawable>& opaqueDrawables, ForwardOpaqueRenderContext& ctx);

struct ForwardOpaquePassBuildParams {
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    rhi::ClearValue clearColor = { 0.f, 0.f, 0.f, 1.f };
    bool setDepthTest = true;
    bool setCullFace = false;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    const std::vector<Drawable>* opaqueDrawables = nullptr;
    std::function<void(rhi::ICommandList*, const RenderFrameData&, const CameraView&, const std::vector<Drawable>&)> drawCallback;
};

struct ForwardOpaquePassExecuteParams {
    ForwardOpaquePassBuildParams build{};
};

class ForwardOpaquePassSystem {
public:
    ForwardOpaquePassExecuteParams BuildParams(const ForwardOpaquePassBuildParams& params) const;
    bool Execute(const ForwardOpaquePassExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

