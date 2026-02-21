#include "eyelib/render/rhi/OpenGL/CommandListGL.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/render/rhi/OpenGL/PipelineGL.h"
#include "eyelib/render/rhi/OpenGL/DescriptorSetGL.h"
#include "eyelib/render/rhi/OpenGL/RenderTargetGL.h"
#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include "eyelib/render/rhi/OpenGL/QueryPoolGL.h"
#include "eyelib/render/rhi/OpenGL/FenceGL.h"
#include "eyelib/render/rhi/OpenGL/GLEnums.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include <glad/glad.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

CommandListGL::CommandListGL(DeviceGL* device) : m_device(device) {
    glGenVertexArrays(1, reinterpret_cast<GLuint*>(&m_vao));
}

CommandListGL::~CommandListGL() {
    if (m_vao)
        glDeleteVertexArrays(1, reinterpret_cast<GLuint*>(&m_vao));
    if (m_bonesUBO && m_device) {
        m_device->DestroyBuffer(m_bonesUBO);
        m_bonesUBO = nullptr;
        m_bonesUBOSize = 0;
    }
}

void CommandListGL::Begin() {
    glBindVertexArray(m_vao);
    // Reset VAO to a known state so no stale vertex attribs leak into compute or the next draw.
    constexpr uint32_t kMaxVertexAttribs = 16;
    for (uint32_t loc = 0; loc < kMaxVertexAttribs; ++loc)
        glDisableVertexAttribArray(loc);
    m_currentPipeline = nullptr;
    m_currentRT = nullptr;
    m_currentVBO = nullptr;
    m_currentIBO = nullptr;
}

void CommandListGL::End() {
    glBindVertexArray(0);
}

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

void CommandListGL::ClearDefaultFramebuffer(const ClearValue& clear) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(clear.color[0], clear.color[1], clear.color[2], clear.color[3]);
    glDepthMask(GL_TRUE);
    glClearDepth(static_cast<double>(clear.depth));
    glClearStencil(static_cast<int>(clear.stencil));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
    if (enable) {
        glEnable(GL_CULL_FACE);
        glCullFace(m_cullFrontFace ? GL_FRONT : GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void CommandListGL::SetCullFrontFace(bool cullFront) {
    m_cullFrontFace = cullFront;
    glCullFace(cullFront ? GL_FRONT : GL_BACK);
}

void CommandListGL::SetBlend(bool enable) {
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

namespace {
GLbitfield ResourceStateToMemoryBarrierBuffer(ResourceState fromState, ResourceState toState) {
    (void)fromState;
    GLbitfield bits = 0;
    switch (toState) {
    case ResourceState::IndirectDraw:
        bits |= GL_COMMAND_BARRIER_BIT;
        /* After compute writing to SSBO, ensure storage writes are visible before indirect read. */
        bits |= GL_SHADER_STORAGE_BARRIER_BIT;
        break;
    case ResourceState::VertexBuffer:
        bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
        break;
    case ResourceState::IndexBuffer:
        bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
        break;
    case ResourceState::UniformBuffer:
    case ResourceState::ShaderResource:
        bits |= GL_UNIFORM_BARRIER_BIT;
        break;
    case ResourceState::TransferSrc:
    case ResourceState::TransferDst:
        bits |= GL_BUFFER_UPDATE_BARRIER_BIT;
        break;
    default:
        break;
    }
    return bits;
}

GLbitfield ResourceStateToMemoryBarrierTexture(ResourceState fromState, ResourceState toState) {
    GLbitfield bits = 0;
    if (toState == ResourceState::ShaderResource || toState == ResourceState::DepthRead) {
        if (fromState == ResourceState::RenderTarget || fromState == ResourceState::DepthWrite)
            bits |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT;
    } else if (toState == ResourceState::RenderTarget || toState == ResourceState::DepthWrite) {
        if (fromState == ResourceState::ShaderResource || fromState == ResourceState::DepthRead)
            bits |= GL_FRAMEBUFFER_BARRIER_BIT;
    }
    if (toState == ResourceState::ShaderResource)
        bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    return bits;
}
} // namespace

void CommandListGL::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    (void)buffer;
    GLbitfield bits = ResourceStateToMemoryBarrierBuffer(fromState, toState);
    if (bits) glMemoryBarrier(bits);
}

void CommandListGL::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    (void)texture;
    GLbitfield bits = ResourceStateToMemoryBarrierTexture(fromState, toState);
    if (bits) {
        glMemoryBarrier(bits);
        /* Same-texture read-after-write: glTextureBarrier() when available (ARB_texture_barrier / GL 4.5). */
        if (GLAD_GL_ARB_texture_barrier &&
            (fromState == ResourceState::RenderTarget || fromState == ResourceState::DepthWrite) &&
            (toState == ResourceState::ShaderResource || toState == ResourceState::DepthRead))
            glTextureBarrier();
    }
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
        GLenum func = GL_LESS;
        switch (d.depthCompare) {
            case DepthCompare::Less:      func = GL_LESS;   break;
            case DepthCompare::LessEqual:  func = GL_LEQUAL; break;
            case DepthCompare::Greater:    func = GL_GREATER; break;
        }
        glDepthFunc(func);
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
    // Compute: unbind VAO to avoid GL 1282 "Wrong component type or count" when binding SSBOs (driver validates vertex state). Graphics: bind VAO and disable all attribs.
    const bool isCompute = m_currentPipeline->m_vertexLayout.attributes.empty();
    if (isCompute) {
        glBindVertexArray(0);
    } else {
        glBindVertexArray(m_vao);
        constexpr uint32_t kMaxVertexAttribs = 16;
        for (uint32_t loc = 0; loc < kMaxVertexAttribs; ++loc)
            glDisableVertexAttribArray(loc);
    }
    glUseProgram(m_currentPipeline->m_program);
    if (m_currentPipeline->m_hasTessellation)
        glPatchParameteri(GL_PATCH_VERTICES, static_cast<GLint>(m_currentPipeline->m_patchVertexCount));
    ApplyBlend(m_currentPipeline->m_blend);
    ApplyDepthStencil(m_currentPipeline->m_depthStencil);
    ApplyRaster(m_currentPipeline->m_raster);
}

void CommandListGL::DrawFullscreenQuad(IPipeline* pipeline) {
    if (!pipeline || !m_device) return;
    m_device->EnsureFullscreenQuad();
    IBuffer* vbo = m_device->GetFullscreenQuadVBO();
    IBuffer* ibo = m_device->GetFullscreenQuadIBO();
    if (!vbo || !ibo) return;
    BindPipeline(pipeline);
    auto* b = static_cast<BufferGL*>(vbo);
    auto* i = static_cast<BufferGL*>(ibo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, b->m_handle);
    constexpr uint32_t kMaxVertexAttribs = 16;
    for (uint32_t loc = 2; loc < kMaxVertexAttribs; ++loc)
        glDisableVertexAttribArray(loc);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    constexpr GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i->m_handle);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
}

void CommandListGL::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    m_currentVBO = static_cast<BufferGL*>(buffer);
    if (!m_currentVBO || !m_currentPipeline) return;
    glBindVertexArray(m_vao);  // Re-bind in case legacy GL code (e.g. fluid renderer) changed it
    glBindBuffer(GL_ARRAY_BUFFER, m_currentVBO->m_handle);
    const VertexLayout& layout = m_currentPipeline->m_vertexLayout;
    constexpr uint32_t kMaxVertexAttribs = 16;
    for (uint32_t loc = 0; loc < kMaxVertexAttribs; ++loc) {
        bool inLayout = false;
        for (const auto& att : layout.attributes) {
            if (att.location == loc) { inLayout = true; break; }
        }
        if (!inLayout)
            glDisableVertexAttribArray(loc);
    }
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
    if (m_currentIBO) {
        glBindVertexArray(m_vao);  // Re-bind in case legacy GL changed it
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_currentIBO->m_handle);
    }
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
    const GLsizeiptr fullSize = static_cast<GLsizeiptr>(b->GetSize());
    const GLsizeiptr rangeSize = size > 0 ? static_cast<GLsizeiptr>(size) : fullSize;
    if (rangeSize <= 0) return;  // glBindBufferRange with size <= 0 is GL_INVALID_VALUE; skip to avoid validation error.
    // Ensure no buffer is bound as draw-indirect when we bind as SSBO (same buffer dual-bound can trigger GL 1282).
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, b->m_handle,
                      static_cast<GLintptr>(offset), rangeSize);
}

void CommandListGL::BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) {
    if (!m_device->GetCapabilities().supportsAtomicCounters) return;
    if (!buffer) return;
    auto* b = static_cast<BufferGL*>(buffer);
    GLsizeiptr sz = size > 0 ? static_cast<GLsizeiptr>(size) : static_cast<GLsizeiptr>(b->GetSize());
    glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, slot, b->m_handle, static_cast<GLintptr>(offset), sz);
}

void CommandListGL::BindTexture(ITexture* texture, uint32_t binding) {
    if (!texture) return;
    auto* t = static_cast<TextureGL*>(texture);
    glBindTextureUnit(binding, t->m_handle);
}

void CommandListGL::UnbindTexture(uint32_t binding) {
    glBindTextureUnit(binding, 0);
}

void CommandListGL::BindDescriptorSet(IDescriptorSet* set) {
    if (!set) return;
    auto* ds = static_cast<DescriptorSetGL*>(set);
    for (const auto& t : ds->textures) {
        if (t.texture) {
            auto* tex = static_cast<TextureGL*>(t.texture);
            glBindTextureUnit(t.binding, tex->m_handle);
        } else {
            glBindTextureUnit(t.binding, 0);
        }
    }
    for (const auto& b : ds->buffers) {
        if (!b.buffer) continue;
        auto* buf = static_cast<BufferGL*>(b.buffer);
        GLsizeiptr sz = b.size > 0 ? static_cast<GLsizeiptr>(b.size) : static_cast<GLsizeiptr>(buf->GetSize());
        if (b.isStorage)
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, b.binding, buf->m_handle, static_cast<GLintptr>(b.offset), sz);
        else
            glBindBufferRange(GL_UNIFORM_BUFFER, b.binding, buf->m_handle, static_cast<GLintptr>(b.offset), sz);
    }
}

void CommandListGL::BindBindlessHeap(IBindlessHeap* heap) {
    (void)heap;
    // OpenGL bindless: handles are made resident on the heap; no per-draw bind. Shaders use handle (uint64) directly.
}

void CommandListGL::SetPushConstants(uint32_t offset, uint32_t size, const void* data) {
    (void)offset;
    (void)size;
    (void)data;
    // OpenGL has no native push constants; use SetUniformMat4/SetUniform* for per-draw data, or a small UBO.
}

static GLint GetUniformLocation(PipelineGL* pipeline, const char* name) {
    if (!pipeline || !pipeline->m_program || !name) return -1;
    return glGetUniformLocation(pipeline->m_program, name);
}

void CommandListGL::SetUniformInt(const char* name, int value) {
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform1i(m_currentPipeline->m_program, loc, value);
}

void CommandListGL::SetUniformUint(const char* name, uint32_t value) {
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0) glProgramUniform1ui(m_currentPipeline->m_program, loc, value);
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
    if (loc < 0) return;
    // Some drivers report GL 1282 for glProgramUniformMatrix4fv on compute programs; use glUniform* (current program) path for compute.
    const bool isCompute = m_currentPipeline->m_vertexLayout.attributes.empty();
    if (isCompute)
        glUniformMatrix4fv(loc, 1, GL_FALSE, data);
    else
        glProgramUniformMatrix4fv(m_currentPipeline->m_program, loc, 1, GL_FALSE, data);
}

void CommandListGL::SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) {
    if (!m_device->GetCapabilities().supportsShaderSubroutines || !m_currentPipeline || !m_currentPipeline->m_program || !indices) return;
    glUniformSubroutinesuiv(GLEnums::ShaderStageToGL(stage), static_cast<GLsizei>(count), indices);
}

void CommandListGL::SetUniformHandle64(const char* name, uint64_t handle) {
    if (!m_device->GetCapabilities().supportsBindless) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0 && GLAD_GL_ARB_bindless_texture)
        glProgramUniformHandleui64ARB(m_currentPipeline->m_program, loc, static_cast<GLuint64>(handle));
}

void CommandListGL::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    if (!data) return;
    // Standard: u_Bones always in UBO at binding Bones (3). Try block first.
    if (name && std::strcmp(name, kBonesUniformName) == 0 && count > 0 &&
        m_currentPipeline && m_currentPipeline->m_program && m_device) {
        const GLuint blockIndex = glGetUniformBlockIndex(m_currentPipeline->m_program, kBonesBlockName);
        if (blockIndex != GL_INVALID_INDEX) {
            const size_t required = count * sizeof(float) * 16;
            if (!m_bonesUBO || m_bonesUBOSize < required) {
                if (m_bonesUBO) {
                    m_device->DestroyBuffer(m_bonesUBO);
                    m_bonesUBO = nullptr;
                    m_bonesUBOSize = 0;
                }
                BufferDesc bd;
                bd.sizeInBytes = required;
                bd.usage = BufferUsage::Uniform;
                bd.memoryHint = BufferMemoryHint::HostVisible;
                m_bonesUBO = m_device->CreateBuffer(bd, nullptr);
                m_bonesUBOSize = m_bonesUBO ? required : 0;
            }
            if (m_bonesUBO) {
                m_bonesUBO->Update(data, 0, required);
                glUniformBlockBinding(m_currentPipeline->m_program, blockIndex, UniformBinding::Bones);
                BindUniformBuffer(m_bonesUBO, UniformBinding::Bones, 0, static_cast<uint32_t>(required));
            }
            return;
        }
    }
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0)
        glProgramUniformMatrix4fv(m_currentPipeline->m_program, loc, static_cast<GLsizei>(count), GL_FALSE, data);
}

void CommandListGL::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    glBindVertexArray(m_vao);  // Re-bind in case legacy GL changed it
    glDrawArrays(GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList),
                 static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
}

void CommandListGL::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    glBindVertexArray(m_vao);  // Re-bind in case legacy GL changed it
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

void CommandListGL::DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) {
    if (!indirectBuffer || !m_device->GetCapabilities().supportsMultiDrawIndirect) return;
    glBindVertexArray(m_vao);
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    glDrawArraysIndirect(mode, reinterpret_cast<const void*>(offset));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) {
    if (!indirectBuffer || !m_device->GetCapabilities().supportsMultiDrawIndirect) return;
    glBindVertexArray(m_vao);
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    glDrawElementsIndirect(mode, idxType, reinterpret_cast<const void*>(offset));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!indirectBuffer || drawCount == 0 || !m_device->GetCapabilities().supportsMultiDrawIndirect) return;
    glBindVertexArray(m_vao);
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    glMultiDrawArraysIndirect(mode, reinterpret_cast<const void*>(offset), static_cast<GLsizei>(drawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!indirectBuffer || drawCount == 0 || !m_device->GetCapabilities().supportsMultiDrawIndirect) return;
    glBindVertexArray(m_vao);
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    glMultiDrawElementsIndirect(mode, idxType, reinterpret_cast<const void*>(offset), static_cast<GLsizei>(drawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!indirectBuffer || !countBuffer || maxDrawCount == 0 || !m_device->GetCapabilities().supportsMultiDrawIndirectCount) return;
    glBindVertexArray(m_vao);
    auto* indBuf = static_cast<BufferGL*>(indirectBuffer);
    auto* cntBuf = static_cast<BufferGL*>(countBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indBuf->m_handle);
    glBindBuffer(GL_PARAMETER_BUFFER_ARB, cntBuf->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    glMultiDrawArraysIndirectCountARB(mode, reinterpret_cast<const void*>(indirectOffset), static_cast<GLintptr>(countBufferOffset), static_cast<GLsizei>(maxDrawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!indirectBuffer || !countBuffer || maxDrawCount == 0 || !m_device->GetCapabilities().supportsMultiDrawIndirectCount) return;
    glBindVertexArray(m_vao);
    auto* indBuf = static_cast<BufferGL*>(indirectBuffer);
    auto* cntBuf = static_cast<BufferGL*>(countBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indBuf->m_handle);
    glBindBuffer(GL_PARAMETER_BUFFER_ARB, cntBuf->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    glMultiDrawElementsIndirectCountARB(mode, idxType, reinterpret_cast<const void*>(indirectOffset), static_cast<GLintptr>(countBufferOffset), static_cast<GLsizei>(maxDrawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_PARAMETER_BUFFER_ARB, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    // glDispatchCompute(0, y, z) is GL_INVALID_OPERATION; clamp to at least 1 per dimension.
    const GLuint x = (groupCountX > 0u) ? groupCountX : 1u;
    const GLuint y = (groupCountY > 0u) ? groupCountY : 1u;
    const GLuint z = (groupCountZ > 0u) ? groupCountZ : 1u;
    glDispatchCompute(x, y, z);
}

void CommandListGL::DispatchIndirect(IBuffer* indirectBuffer, size_t offset) {
    if (!indirectBuffer || !m_device->GetCapabilities().supportsCompute) return;
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, b->m_handle);
    glDispatchComputeIndirect(static_cast<GLintptr>(offset));
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
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

void CommandListGL::BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) {
    if (slot >= kMaxTransformFeedbackBuffers) return;
    m_tfBuffers[slot].buffer = buffer ? static_cast<BufferGL*>(buffer) : nullptr;
    m_tfBuffers[slot].offset = offset;
    m_tfBuffers[slot].size = size;
}

void CommandListGL::BeginTransformFeedback(PrimitiveTopology primitiveMode) {
    if (!m_device->GetCapabilities().supportsTransformFeedback) return;
    for (uint32_t i = 0; i < kMaxTransformFeedbackBuffers; ++i) {
        if (m_tfBuffers[i].buffer && m_tfBuffers[i].buffer->m_handle) {
            GLsizeiptr sz = m_tfBuffers[i].size > 0 ? static_cast<GLsizeiptr>(m_tfBuffers[i].size)
                                                    : static_cast<GLsizeiptr>(m_tfBuffers[i].buffer->GetSize());
            glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, i, m_tfBuffers[i].buffer->m_handle,
                static_cast<GLintptr>(m_tfBuffers[i].offset), sz);
        }
    }
    GLenum mode = GLEnums::PrimitiveTopologyToGL(primitiveMode);
    glBeginTransformFeedback(mode);
}

void CommandListGL::EndTransformFeedback() {
    if (!m_device->GetCapabilities().supportsTransformFeedback) return;
    glEndTransformFeedback();
}

void CommandListGL::BeginConditionalRender(IQueryPool* pool, uint32_t index) {
    if (!m_device->GetCapabilities().supportsConditionalRender || !pool || m_inConditionalRender) return;
    auto* q = static_cast<QueryPoolGL*>(pool);
    if (index >= q->m_queryIds.size()) return;
    glBeginConditionalRender(q->m_queryIds[index], GL_QUERY_WAIT);
    m_inConditionalRender = true;
}

void CommandListGL::EndConditionalRender() {
    if (!m_inConditionalRender) return;
    glEndConditionalRender();
    m_inConditionalRender = false;
}

IFence* CommandListGL::InsertFence() {
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!sync) return nullptr;
    auto* f = new FenceGL();
    f->m_sync = static_cast<void*>(sync);
    return f;
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
