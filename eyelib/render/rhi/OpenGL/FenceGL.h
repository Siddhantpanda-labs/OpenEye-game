#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct FenceGL : IFence {
    void* m_sync = nullptr;  // GLsync (opaque; avoid including GL in header)
};

} // namespace rhi
} // namespace render
} // namespace eyelib
