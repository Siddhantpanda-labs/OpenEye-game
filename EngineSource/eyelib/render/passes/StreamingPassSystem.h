#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

struct StreamingNv12BuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IPipeline* pipeline = nullptr;
    rhi::IRenderTarget* sourceRT = nullptr;
    void* nv12BufferHandle = nullptr;
    size_t nv12Size = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
    bool useTypedPush = false;
};

struct StreamingNv12ExecuteParams {
    StreamingNv12BuildParams build{};
};

class StreamingPassSystem {
public:
    StreamingNv12ExecuteParams BuildNv12Params(const StreamingNv12BuildParams& params) const;
    bool ExecuteNv12(const StreamingNv12ExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

