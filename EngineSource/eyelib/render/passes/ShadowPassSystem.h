#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

struct ShadowPassBuildParams {
    rhi::ICommandList* cmd = nullptr;
    const RenderFrameData* frameData = nullptr;
    CameraView camView{};
    bool enablePass = false;
    std::function<void(rhi::ICommandList*, const RenderFrameData&, const CameraView&)> drawCallback;
};

struct ShadowPassExecuteParams {
    ShadowPassBuildParams build{};
};

class ShadowPassSystem {
public:
    ShadowPassExecuteParams BuildParams(const ShadowPassBuildParams& params) const;
    bool Execute(const ShadowPassExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

