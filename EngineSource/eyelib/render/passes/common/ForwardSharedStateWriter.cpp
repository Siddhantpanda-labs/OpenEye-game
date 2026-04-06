#include "eyelib/render/passes/common/ForwardSharedStateWriter.h"

#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/ibl/IBL.h"

#include <cstring>

namespace eyelib {
namespace render {
namespace passes {
namespace common {

const ReflectionProbe* ResolveBestReflectionProbe(ReflectionProbeManager* reflectionProbeManager, const math::Vec3& viewPos) {
    if (!reflectionProbeManager) return nullptr;
    const ReflectionProbe* probe = reflectionProbeManager->FindClosestProbe(viewPos);
    if (!probe) probe = reflectionProbeManager->GetGlobalProbe();
    return probe;
}

bool WriteForwardSharedState(const ForwardSharedStateWriteParams& params) {
    if (!params.passSet || !params.sceneSet || !params.frameUbo || !params.light || !params.camera || !params.lightSpaceMatrix) return false;

    FillSceneLightingBuffer(
        params.sceneLightingBuffer,
        *params.light,
        params.camera->viewPos,
        *params.lightSpaceMatrix,
        0.005f,
        params.useShadows,
        params.pointLights,
        params.spotLights,
        params.cascadeShadowMap);

    math::Mat4 viewProjection = params.camera->projection * params.camera->view;
    float frameUboData[5 * 16];
    std::memcpy(frameUboData, &params.camera->view[0][0], 16 * sizeof(float));
    std::memcpy(frameUboData + 16, &params.camera->projection[0][0], 16 * sizeof(float));
    std::memcpy(frameUboData + 32, &viewProjection[0][0], 16 * sizeof(float));
    if (params.usePrevViewProj && params.prevView && params.prevProjection) {
        std::memcpy(frameUboData + 48, &(*params.prevView)[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 64, &(*params.prevProjection)[0][0], 16 * sizeof(float));
    } else {
        std::memcpy(frameUboData + 48, &params.camera->view[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 64, &params.camera->projection[0][0], 16 * sizeof(float));
    }
    params.frameUbo->Update(frameUboData, 0, 320);

    // Set 1 binding 0 is sampler2DArray in forward.frag; must always be written (IBL uses nullptr→fallback 2D/cube only).
    rhi::ITexture* shadowForBinding0 = nullptr;
    if (params.useShadows && params.shadowDepthTexture)
        shadowForBinding0 = params.shadowDepthTexture;
    else if (params.shadowMapArrayFallback)
        shadowForBinding0 = params.shadowMapArrayFallback;
    else if (params.cascadeShadowMap)
        shadowForBinding0 = params.cascadeShadowMap->GetDepthTextureRHI();
    params.passSet->UpdateTexture(0, shadowForBinding0);
    if (params.sceneLightingBuffer) {
        params.passSet->UpdateStorageBuffer(1, params.sceneLightingBuffer, 0, 0);
        rhi::IBuffer* clusterOrFallback = params.lightIndexBuffer ? params.lightIndexBuffer : params.sceneLightingBuffer;
        params.passSet->UpdateStorageBuffer(2, clusterOrFallback, 0, 0);
        rhi::IBuffer* sceneFallback = params.sceneLightingBuffer;
        rhi::IBuffer* bonesOrFallback = params.bonesBuffer ? params.bonesBuffer : sceneFallback;
        params.sceneSet->UpdateStorageBuffer(0, sceneFallback, 0, 0);
        params.sceneSet->UpdateStorageBuffer(1, bonesOrFallback, 0, 0);
        params.sceneSet->UpdateStorageBuffer(2, params.sceneLightingBuffer, 0, 0);
        params.sceneSet->UpdateStorageBuffer(3, clusterOrFallback, 0, 0);
    }

    // Always bind (nullptr → DescriptorSetVK uses correct 2D/cube fallback per binding type).
    params.passSet->UpdateTexture(3, params.irradianceTexture);
    params.passSet->UpdateTexture(4, params.prefilterTexture);
    params.passSet->UpdateTexture(5, params.brdfLutTexture);
    return true;
}

ForwardSharedEnvironmentState ResolveForwardSharedEnvironment(const ForwardSharedEnvironmentResolveParams& params) {
    ForwardSharedEnvironmentState out{};
    if (!params.light || !params.camera) return out;

    const ReflectionProbe* environmentProbe = ResolveBestReflectionProbe(params.reflectionProbeManager, params.camera->viewPos);

    out.useShadows = (params.cascadeShadowMap != nullptr && params.light->castShadows);
    if (params.light->castShadows && params.cascadeShadowMap && params.cascadeShadowMap->GetSplitCount() > 0)
        out.lightSpaceMatrix = params.cascadeShadowMap->GetLightSpaceMatrix(0);

    out.shadowDepthTexture = out.useShadows && params.cascadeShadowMap ? params.cascadeShadowMap->GetDepthTextureRHI() : nullptr;
    out.irradianceTexture = environmentProbe && environmentProbe->IsValid()
                                ? environmentProbe->GetIrradianceMap()
                                : (params.ibl && params.ibl->IsValid() ? params.ibl->GetIrradianceTextureRHI() : nullptr);
    out.prefilterTexture = environmentProbe && environmentProbe->IsValid()
                               ? environmentProbe->GetPrefilteredMap()
                               : (params.ibl && params.ibl->IsValid() ? params.ibl->GetPrefilterTextureRHI() : nullptr);
    out.brdfLutTexture = (params.ibl && params.ibl->IsValid()) ? params.ibl->GetBRDFLUTTextureRHI() : nullptr;
    return out;
}

}  // namespace common
}  // namespace passes
}  // namespace render
}  // namespace eyelib

