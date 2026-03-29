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
class ClusteredLightGrid;
class LightCullPassSystem;
class Shader;
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

/** Per-frame state and callbacks for stereo forward opaque rendering. */
struct ForwardStereoRenderContext {
    rhi::IDevice* device = nullptr;
    ShaderCache* shaderCache = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    CascadeShadowMap* cascadeShadowMap = nullptr;
    std::function<void()> ensureStereoForwardShader;
    std::function<Shader*()> getStereoForwardShader;
    std::function<void(rhi::ICommandList*, const RenderFrameData&, const CameraView&, const std::vector<Drawable>&)> renderForwardOpaqueForCamera;
};

void ForwardOpaqueRenderStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam,
                               ForwardStereoRenderContext& ctx);

/** Per-frame state and acquire hooks for Forward+ opaque batched draws. */
struct ForwardPlusOpaqueRenderContext {
    rhi::IDevice* device = nullptr;
    ShaderCache* shaderCache = nullptr;
    resources::PassResourceBuilder* passResourceBuilder = nullptr;
    rhi::IBuffer* frameUbo = nullptr;
    rhi::IBuffer* bonesBuffer = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    rhi::IBuffer* lightIndexBuffer = nullptr;
    rhi::IBuffer* instanceMatrixBuffer = nullptr;
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

void ForwardPlusOpaqueRenderBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData, ForwardPlusOpaqueRenderContext& ctx);

/** Per-frame state for Forward+ frame setup (light buffers, cull, clear). */
struct ForwardPlusPrepareContext {
    rhi::IDevice* device = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    ClusteredLightGrid* clusteredLightGrid = nullptr;
    LightCullPassSystem* lightCullPassSystem = nullptr;
    rhi::IBuffer** pointLightBuffer = nullptr;
    rhi::IBuffer** spotLightBuffer = nullptr;
    rhi::IBuffer** lightIndexBuffer = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    CascadeShadowMap* cascadeShadowMap = nullptr;
};

void ForwardPlusPrepareFrame(rhi::ICommandList* cmd, const RenderFrameData& frameData, ForwardPlusPrepareContext& ctx);

/** Per-frame state and acquire hooks for Vulkan forward transparent draws. */
struct ForwardTransparentRenderContext {
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

void ForwardTransparentRender(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                              ForwardTransparentRenderContext& ctx);

}  // namespace render
}  // namespace eyelib

