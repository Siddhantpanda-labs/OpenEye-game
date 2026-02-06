#include "eyelib/render/rhi/OpenGL/CommandListGL.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/render/rhi/OpenGL/PipelineGL.h"
#include "eyelib/render/rhi/OpenGL/RenderTargetGL.h"
#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include "eyelib/render/rhi/OpenGL/QueryPoolGL.h"
#include "eyelib/render/rhi/OpenGL/GLEnums.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

CommandListGL::CommandListGL(DeviceGL* device) : m_device(device) {}

CommandListGL::~CommandListGL() = default;

void CommandListGL::Begin() {
    m_currentPipeline = nullptr;
    m_currentRT = nullptr;
    m_currentVBO = nullptr;
    m_currentIBO = nullptr;
}

void CommandListGL::End() {}

void CommandListGL::SetRenderTarget(IRenderTarget* rt) {
    m_currentRT = rt;
    if (!rt) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    auto* r = static_cast<RenderTargetGL*>(rt);
    glBindFramebuffer(GL_FRAMEBUFFER, r->m_fbo);
}

void CommandListGL::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListGL::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    if (!rt) return;
    SetRenderTarget(rt);
    auto* r = static_cast<RenderTargetGL*>(rt);
    if (r->GetColorTexture(0))
        glClearBufferfv(GL_COLOR, 0, clear.color);
    if (r->GetDepthTexture()) {
        glDepthMask(GL_TRUE);
        glClearBufferfv(GL_DEPTH, 0, &clear.depth);
    }
}

void CommandListGL::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void CommandListGL::SetDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

void CommandListGL::SetDepthWrite(bool enable) {
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
}

void CommandListGL::SetDepthFuncLess(bool less) {
    glDepthFunc(less ? GL_LESS : GL_LEQUAL);
}

void CommandListGL::SetCullFace(bool enable) {
    if (enable) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void CommandListGL::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    (void)buffer;
    (void)fromState;
    (void)toState;
    // OpenGL: no explicit buffer barriers for our usage.
}

void CommandListGL::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    (void)texture;
    (void)fromState;
    (void)toState;
    // OpenGL: layout is implicit; glMemoryBarrier if needed for image load/store.
}

static void ApplyBlend(const BlendState& b) {
    if (!b.blendEnable) {
        glDisable(GL_BLEND);
        return;
    }
    glEnable(GL_BLEND);
    // Simplified: use same factor for color and alpha
    GLenum src = GL_ONE, dst = GL_ZERO;
    if (b.srcColor == BlendFactor::SrcAlpha) src = GL_SRC_ALPHA;
    if (b.dstColor == BlendFactor::OneMinusSrcAlpha) dst = GL_ONE_MINUS_SRC_ALPHA;
    glBlendFunc(src, dst);
}

static void ApplyDepthStencil(const DepthStencilState& d) {
    if (d.depthTest) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(d.depthCompareLess ? GL_LESS : GL_GREATER);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(d.depthWrite ? GL_TRUE : GL_FALSE);
}

static void ApplyRaster(const RasterState& r) {
    if (r.cullEnable) {
        glEnable(GL_CULL_FACE);
        glFrontFace(r.frontCounterClockwise ? GL_CCW : GL_CW);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
    if (r.depthBiasEnable)
        glPolygonOffset(r.depthBiasSlope, r.depthBiasConstant);
    else
        glPolygonOffset(0.f, 0.f);
}

void CommandListGL::BindPipeline(IPipeline* pipeline) {
    m_currentPipeline = static_cast<PipelineGL*>(pipeline);
    if (!m_currentPipeline) return;
    glUseProgram(m_currentPipeline->m_program);
    ApplyBlend(m_currentPipeline->m_blend);
    ApplyDepthStencil(m_currentPipeline->m_depthStencil);
    ApplyRaster(m_currentPipeline->m_raster);
}

void CommandListGL::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    m_currentVBO = static_cast<BufferGL*>(buffer);
    if (!m_currentVBO || !m_currentPipeline) return;
    glBindBuffer(GL_ARRAY_BUFFER, m_currentVBO->m_handle);
    const VertexLayout& layout = m_currentPipeline->m_vertexLayout;
    for (const auto& att : layout.attributes) {
        glEnableVertexAttribArray(att.location);
        GLenum glType = GLEnums::VertexAttributeTypeToGL(att.type);
        int count = GLEnums::VertexAttributeTypeCount(att.type);
        bool normalized = (glType == GL_FLOAT);
        if (glType == GL_INT || glType == GL_UNSIGNED_INT)
            glVertexAttribIPointer(att.location, count, glType,
                                  static_cast<GLsizei>(layout.strideInBytes),
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(att.offsetInBytes + offset)));
        else
            glVertexAttribPointer(att.location, count, glType, normalized,
                                  static_cast<GLsizei>(layout.strideInBytes),
                                  reinterpret_cast<void*>(static_cast<uintptr_t>(att.offsetInBytes + offset)));
    }
}

void CommandListGL::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    m_currentIBO = static_cast<BufferGL*>(buffer);
    m_currentIndexType = indexType;
    if (m_currentIBO)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_currentIBO->m_handle);
    (void)offset;
}

void CommandListGL::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    if (!buffer) return;
    auto* b = static_cast<BufferGL*>(buffer);
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, b->m_handle,
                      static_cast<GLintptr>(offset),
                      size > 0 ? static_cast<GLsizeiptr>(size) : static_cast<GLsizeiptr>(b->GetSize()));
}

void CommandListGL::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    if (!buffer) return;
    auto* b = static_cast<BufferGL*>(buffer);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, b->m_handle,
                      static_cast<GLintptr>(offset),
                      size > 0 ? static_cast<GLsizeiptr>(size) : static_cast<GLsizeiptr>(b->GetSize()));
}

void CommandListGL::BindTexture(ITexture* texture, uint32_t binding) {
    if (!texture) return;
    auto* t = static_cast<TextureGL*>(texture);
    glBindTextureUnit(binding, t->m_handle);
}

void CommandListGL::UnbindTexture(uint32_t binding) {
    glBindTextureUnit(binding, 0);
}

static GLint GetUniformLocation(PipelineGL* pipeline, const char* name) {
    if (!pipeline || !pipeline->m_program || !name) return -1;
    return glGetUniformLocation(pipeline->m_program, name);
}

void CommandListGL::SetUniformInt(const char* name, int value) {
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform1i(m_currentPipeline->m_program, loc, value);
}

void CommandListGL::SetUniformFloat(const char* name, float value) {
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform1f(m_currentPipeline->m_program, loc, value);
}

void CommandListGL::SetUniformVec2(const char* name, const float* data) {
    if (!data) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform2fv(m_currentPipeline->m_program, loc, 1, data);
}

void CommandListGL::SetUniformVec3(const char* name, const float* data) {
    if (!data) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform3fv(m_currentPipeline->m_program, loc, 1, data);
}

void CommandListGL::SetUniformVec4(const char* name, const float* data) {
    if (!data) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform4fv(m_currentPipeline->m_program, loc, 1, data);
}

void CommandListGL::SetUniformMat4(const char* name, const float* data) {
    if (!data) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniformMatrix4fv(m_currentPipeline->m_program, loc, 1, GL_FALSE, data);
}

void CommandListGL::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    if (!data) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniformMatrix4fv(m_currentPipeline->m_program, loc, static_cast<GLsizei>(count), GL_FALSE, data);
}

void CommandListGL::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    glDrawArrays(GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList),
                 static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
}

void CommandListGL::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    size_t indexSize = (m_currentIndexType == IndexType::UInt16) ? 2 : 4;
    const void* offsetPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex * indexSize));
    glDrawElementsBaseVertex(
        GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList),
        static_cast<GLsizei>(indexCount), idxType, offsetPtr, vertexOffset);
}

void CommandListGL::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}

void CommandListGL::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    glDispatchCompute(groupCountX, groupCountY, groupCountZ);
}

void CommandListGL::BeginQuery(IQueryPool* pool, uint32_t index) {
    auto* p = static_cast<QueryPoolGL*>(pool);
    if (!p || index >= p->m_queryIds.size()) return;
    GLenum type = (p->m_type == QueryType::Timestamp) ? GL_TIME_ELAPSED : GL_TIME_ELAPSED;
    glBeginQuery(type, p->m_queryIds[index]);
}

void CommandListGL::EndQuery(IQueryPool* pool, uint32_t index) {
    auto* p = static_cast<QueryPoolGL*>(pool);
    if (!p || index >= p->m_queryIds.size()) return;
    GLenum type = (p->m_type == QueryType::Timestamp) ? GL_TIME_ELAPSED : GL_TIME_ELAPSED;
    glEndQuery(type);
}

void CommandListGL::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) {
    if (!src || !dst) return;
    auto* s = static_cast<BufferGL*>(src);
    auto* d = static_cast<BufferGL*>(dst);
    if (size == 0) size = s->GetSize();
    glCopyNamedBufferSubData(s->m_handle, d->m_handle,
                             static_cast<GLintptr>(srcOffset),
                             static_cast<GLintptr>(dstOffset),
                             static_cast<GLsizeiptr>(size));
}

} // namespace rhi
} // namespace render
} // namespace eyelib
