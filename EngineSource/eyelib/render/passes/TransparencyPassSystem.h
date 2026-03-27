#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

class ShaderCache;

struct TransparencyPassBuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    rhi::IRenderTarget* gbufferRenderTarget = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    bool preferVulkanPath = false;
    ShaderCache* shaderCache = nullptr;
    std::function<void(rhi::ICommandList*, const RenderFrameData&, const CameraView&)> drawVulkan;
    std::function<void(rhi::IDevice*, rhi::ICommandList*, const std::vector<Drawable>&, const CameraView&, ShaderCache*)> drawLegacy;
};

struct TransparencyPassExecuteParams {
    TransparencyPassBuildParams build{};
};

class TransparencyPassSystem {
public:
    TransparencyPassExecuteParams BuildParams(const TransparencyPassBuildParams& params) const;
    bool Execute(const TransparencyPassExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

