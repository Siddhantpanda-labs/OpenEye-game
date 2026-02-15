#include "eyelib/render/rhi/OpenGL/BindlessHeapGL.h"
#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

BindlessHeapGL::~BindlessHeapGL() {
    if (GLAD_GL_ARB_bindless_texture) {
        for (BindlessHandle h : m_residentTextureHandles) {
            if (h != 0) glMakeTextureHandleNonResidentARB(static_cast<GLuint64>(h));
        }
    }
    m_residentTextureHandles.clear();
    if (GLAD_GL_NV_shader_buffer_load) {
        for (const auto& [addr, bufferId] : m_residentBufferHandles) {
            (void)addr;
            if (bufferId != 0) glMakeNamedBufferNonResidentNV(bufferId);
        }
    }
    m_residentBufferHandles.clear();
}

BindlessHandle BindlessHeapGL::AllocateTextureHandle(ITexture* texture) {
    if (!texture || !GLAD_GL_ARB_bindless_texture) return kInvalidBindlessHandle;
    auto* tex = static_cast<TextureGL*>(texture);
    if (tex->m_handle == 0) return kInvalidBindlessHandle;
    GLuint64 handle = glGetTextureHandleARB(tex->m_handle);
    if (handle == 0) return kInvalidBindlessHandle;
    glMakeTextureHandleResidentARB(handle);
    m_residentTextureHandles.insert(static_cast<BindlessHandle>(handle));
    return static_cast<BindlessHandle>(handle);
}

BindlessHandle BindlessHeapGL::AllocateBufferHandle(IBuffer* buffer) {
    if (!buffer || !GLAD_GL_NV_shader_buffer_load) return kInvalidBindlessHandle;
    auto* buf = static_cast<BufferGL*>(buffer);
    if (buf->m_handle == 0) return kInvalidBindlessHandle;
    GLuint64 gpuAddr = 0;
    glGetNamedBufferParameterui64vNV(buf->m_handle, GL_BUFFER_GPU_ADDRESS_NV, &gpuAddr);
    if (gpuAddr == 0) return kInvalidBindlessHandle;
    glMakeNamedBufferResidentNV(buf->m_handle, GL_READ_ONLY);
    BindlessHandle h = static_cast<BindlessHandle>(gpuAddr);
    m_residentBufferHandles[h] = buf->m_handle;
    return h;
}

void BindlessHeapGL::FreeTextureHandle(BindlessHandle handle) {
    if (handle == kInvalidBindlessHandle) return;
    auto it = m_residentTextureHandles.find(handle);
    if (it == m_residentTextureHandles.end()) return;
    if (GLAD_GL_ARB_bindless_texture)
        glMakeTextureHandleNonResidentARB(static_cast<GLuint64>(handle));
    m_residentTextureHandles.erase(it);
}

void BindlessHeapGL::FreeBufferHandle(BindlessHandle handle) {
    if (handle == kInvalidBindlessHandle) return;
    auto it = m_residentBufferHandles.find(handle);
    if (it == m_residentBufferHandles.end()) return;
    if (GLAD_GL_NV_shader_buffer_load && it->second != 0)
        glMakeNamedBufferNonResidentNV(it->second);
    m_residentBufferHandles.erase(it);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
