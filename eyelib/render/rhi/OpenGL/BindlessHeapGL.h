#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

/** OpenGL bindless heap: GL_ARB_bindless_texture (textures), GL_NV_shader_buffer_load (buffers). */
struct BindlessHeapGL : IBindlessHeap {
    BindlessHeapGL() = default;
    ~BindlessHeapGL() override;

    BindlessHandle AllocateTextureHandle(ITexture* texture) override;
    BindlessHandle AllocateBufferHandle(IBuffer* buffer) override;
    void FreeTextureHandle(BindlessHandle handle) override;
    void FreeBufferHandle(BindlessHandle handle) override;

    /** Texture handles that are currently resident (for cleanup on destroy). */
    std::unordered_set<BindlessHandle> m_residentTextureHandles;
    /** Buffer bindless: GPU address -> GL buffer name (for MakeNamedBufferNonResidentNV on free). */
    std::unordered_map<BindlessHandle, unsigned int> m_residentBufferHandles;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
