#include "eyelib/render/passes/StreamingPassSystem.h"
#include "eyelib/render/PassPushConstants.h"

namespace eyelib {
namespace render {

StreamingNv12ExecuteParams StreamingPassSystem::BuildNv12Params(const StreamingNv12BuildParams& params) const {
    StreamingNv12ExecuteParams out{};
    out.build = params;
    return out;
}

bool StreamingPassSystem::ExecuteNv12(const StreamingNv12ExecuteParams& params) const {
    if (!params.build.device || !params.build.cmd || !params.build.pipeline || !params.build.sourceRT ||
        !params.build.nv12BufferHandle || params.build.nv12Size == 0 || params.build.pitch == 0) {
        return false;
    }
    rhi::ITexture* tex = params.build.sourceRT->GetColorTexture(0);
    if (!tex) return false;

    params.build.cmd->BindPipeline(params.build.pipeline);
    params.build.device->EnsureComputePipelineStorageBlockBindings(params.build.pipeline);
    params.build.cmd->BindTexture(tex, 0);
    params.build.cmd->BindPixelPackBufferAsSSBO(params.build.nv12BufferHandle, 0, params.build.nv12Size);
    if (params.build.useTypedPush) {
        abi::WriteRgbaToNv12PushConstants(params.build.cmd, params.build.width, params.build.height, params.build.pitch);
    } else {
        params.build.cmd->SetUniformUint("u_Width", params.build.width);
        params.build.cmd->SetUniformUint("u_Height", params.build.height);
        params.build.cmd->SetUniformUint("u_Pitch", params.build.pitch);
    }
    const uint32_t gx = (params.build.width + 15u) / 16u;
    const uint32_t gy = (params.build.height + 15u) / 16u;
    params.build.cmd->Dispatch(gx > 0 ? gx : 1u, gy > 0 ? gy : 1u, 1u);
    params.build.cmd->MemoryBarrierStorage();
    return true;
}

}  // namespace render
}  // namespace eyelib

