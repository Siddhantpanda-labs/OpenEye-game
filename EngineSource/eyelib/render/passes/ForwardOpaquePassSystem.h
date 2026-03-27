#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

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

