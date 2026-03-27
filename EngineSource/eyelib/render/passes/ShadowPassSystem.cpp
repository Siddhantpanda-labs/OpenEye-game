#include "eyelib/render/passes/ShadowPassSystem.h"

namespace eyelib {
namespace render {

ShadowPassExecuteParams ShadowPassSystem::BuildParams(const ShadowPassBuildParams& params) const {
    ShadowPassExecuteParams out{};
    out.build = params;
    return out;
}

bool ShadowPassSystem::Execute(const ShadowPassExecuteParams& params) const {
    if (!params.build.frameData || !params.build.cmd || !params.build.enablePass) return false;
    if (params.build.drawCallback)
        params.build.drawCallback(params.build.cmd, *params.build.frameData, params.build.camView);
    return true;
}

}  // namespace render
}  // namespace eyelib

