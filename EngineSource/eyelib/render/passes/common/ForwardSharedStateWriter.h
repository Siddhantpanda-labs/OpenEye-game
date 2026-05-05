#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

class CascadeShadowMap;
class ReflectionProbeManager;
class IBL;
class ReflectionProbe;

namespace passes {
namespace common {

/**
 * Input payload for writing shared forward-family pass state into descriptor sets and frame UBO.
 */
struct ForwardSharedStateWriteParams {
    rhi::IDescriptorSet* passSet = nullptr;
    rhi::IDescriptorSet* sceneSet = nullptr;
    rhi::IBuffer* frameUbo = nullptr;
    rhi::IBuffer* sceneLightingBuffer = nullptr;
    rhi::IBuffer* lightIndexBuffer = nullptr;
    rhi::IBuffer* bonesBuffer = nullptr;
    const LightParams* light = nullptr;
    const CameraView* camera = nullptr;
    const std::vector<PointLight>* pointLights = nullptr;
    const std::vector<SpotLight>* spotLights = nullptr;
    const math::Mat4* prevView = nullptr;
    const math::Mat4* prevProjection = nullptr;
    const math::Mat4* lightSpaceMatrix = nullptr;
    const CascadeShadowMap* cascadeShadowMap = nullptr;
    /** Depth from a 1×1×1 shadow map array when directional shadows are off; matches forward u_ShadowMap (sampler2DArray). */
    rhi::ITexture* shadowMapArrayFallback = nullptr;
    rhi::ITexture* shadowDepthTexture = nullptr;
    rhi::ITexture* irradianceTexture = nullptr;
    rhi::ITexture* prefilterTexture = nullptr;
    rhi::ITexture* brdfLutTexture = nullptr;
    bool useShadows = false;
    bool usePrevViewProj = false;
};

/** Inputs for resolving forward-family shared environment textures and shadow state. */
struct ForwardSharedEnvironmentResolveParams {
    const LightParams* light = nullptr;
    const CameraView* camera = nullptr;
    CascadeShadowMap* cascadeShadowMap = nullptr;
    ReflectionProbeManager* reflectionProbeManager = nullptr;
    IBL* ibl = nullptr;
};

/** Resolved forward-family environment state (shadow + IBL/probe textures). */
struct ForwardSharedEnvironmentState {
    bool useShadows = false;
    math::Mat4 lightSpaceMatrix = math::Mat4(1.0f);
    rhi::ITexture* shadowDepthTexture = nullptr;
    rhi::ITexture* irradianceTexture = nullptr;
    rhi::ITexture* prefilterTexture = nullptr;
    rhi::ITexture* brdfLutTexture = nullptr;
};

/**
 * Writes shared pass/scene descriptor bindings and frame UBO content for forward and forward+.
 * @param params Explicit write inputs collected by orchestrator.
 * @return true when required inputs were valid and writes were applied.
 */
bool WriteForwardSharedState(const ForwardSharedStateWriteParams& params);

/**
 * Resolves shadow/light-space and environment textures for forward-family pass state.
 * @param params Explicit environment sources from orchestrator.
 * @return Resolved environment state used by WriteForwardSharedState.
 */
ForwardSharedEnvironmentState ResolveForwardSharedEnvironment(const ForwardSharedEnvironmentResolveParams& params);

/**
 * Resolves closest valid reflection probe, falling back to global probe when needed.
 * @param reflectionProbeManager Reflection probe manager (nullable).
 * @param viewPos Camera/view position used for nearest-probe lookup.
 * @return Best available probe pointer, or null if none exist.
 */
const ReflectionProbe* ResolveBestReflectionProbe(ReflectionProbeManager* reflectionProbeManager, const math::Vec3& viewPos);

}  // namespace common
}  // namespace passes
}  // namespace render
}  // namespace eyelib

