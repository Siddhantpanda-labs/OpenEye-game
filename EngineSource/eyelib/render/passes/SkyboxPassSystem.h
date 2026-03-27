#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

class Shader;

struct SkyboxPassBuildParams {
    Shader* shader = nullptr;
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* renderTarget = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    rhi::IRenderTarget* gbufferRenderTarget = nullptr;
    rhi::IDescriptorSet* frameSet = nullptr;
    rhi::IDescriptorSet* postSet = nullptr;
    CameraView camera{};
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    bool useGbufferDepth = false;
    bool isVulkan = false;
};

struct SkyboxPassExecuteParams {
    SkyboxPassBuildParams build{};
    math::Mat4 inverseView = math::Mat4(1.0f);
    math::Mat4 inverseProjection = math::Mat4(1.0f);
};

class SkyboxPassSystem {
public:
    SkyboxPassExecuteParams BuildParams(const SkyboxPassBuildParams& params) const;
    bool ExecuteCubemap(const SkyboxPassExecuteParams& params);
    bool ExecuteProcedural(const SkyboxPassExecuteParams& params);
};

}  // namespace render
}  // namespace eyelib

