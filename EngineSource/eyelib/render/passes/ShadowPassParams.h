#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

class ShadowPass;
class CascadeShadowMap;

/** Graph / orchestrator input for the shadow pass (enablement + cmd + camera for shadow projection). */
struct ShadowPassBuildParams {
    rhi::ICommandList* cmd = nullptr;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    bool enablePass = false;
};

/** Default cap for directional shadow draw distance (paired with camera far). */
inline constexpr float kMaxDirectionalShadowDrawDistance = 180.0f;

/** GPU submission payload for batched cascade shadow map rendering. */
struct ShadowPassBatchedParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    ShadowPass* shadowPass = nullptr;
    CascadeShadowMap* cascadeShadowMap = nullptr;
    rhi::IBuffer* instanceMatrixBuffer = nullptr;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    float maxDirectionalShadowDistance = kMaxDirectionalShadowDrawDistance;
};

}  // namespace render
}  // namespace eyelib
