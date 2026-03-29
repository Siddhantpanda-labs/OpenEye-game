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
#include "eyelib/render/rhi/OpenGL/OpenGLResourceSpaceBindings.h"
#include "eyelib/render/rhi/OpenGL/PipelineLayoutGL.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/debug/Validation.h"
#include <glad/glad.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

namespace {
BindingType LookupBindingType(const PipelineLayoutGL* layout, uint32_t logical) {
    if (!layout) return BindingType::Texture;
    for (const auto& b : layout->desc.bindings) {
        if (b.binding == logical) return b.type;
    }
    return BindingType::Texture;
}
} // namespace

CommandListGL::CommandListGL(DeviceGL* device) : m_device(device), m_blendSrc(GL_SRC_ALPHA), m_blendDst(GL_ONE_MINUS_SRC_ALPHA) {
}

CommandListGL::~CommandListGL() {
    if (m_bonesUBO && m_device) {
        m_device->DestroyBuffer(m_bonesUBO);
        m_bonesUBO = nullptr;
        m_bonesUBOSize = 0;
    }
}

void CommandListGL::Begin() {
    m_currentPipeline = nullptr;
    m_currentRT = nullptr;
    m_currentVBO = nullptr;
    m_currentVBOOffset = 0;
    m_currentIBO = nullptr;
    m_boundProgram = 0;
    m_boundVAO = 0;
}

void CommandListGL::End() {
    glBindVertexArray(0);
    m_boundVAO = 0;
}

void CommandListGL::SetRenderTarget(IRenderTarget* rt) {
    m_currentRT = rt;
    if (!rt) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    auto* r = static_cast<RenderTargetGL*>(rt);
    glBindFramebuffer(GL_FRAMEBUFFER, r->m_fbo);
    ITexture* depthTex = r->GetDepthTexture();
    if (depthTex && depthTex->GetType() == TextureType::Texture2DArray) {
        auto* tex = static_cast<TextureGL*>(depthTex);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex->m_handle, 0, r->GetDepthLayer());
    }
}

void CommandListGL::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListGL::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    if (!rt) return;
    SetRenderTarget(rt);
    auto* r = static_cast<RenderTargetGL*>(rt);
    // Clear all attached color targets (MRT), not just COLOR0.
    for (uint32_t i = 0;; ++i) {
        ITexture* color = r->GetColorTexture(i);
        if (!color) break;
        if (color->GetFormat() == RHITextureFormat::R32UI)
            glClearBufferuiv(GL_COLOR, static_cast<GLint>(i), &clear.colorUint);
        else
            glClearBufferfv(GL_COLOR, static_cast<GLint>(i), clear.color);
    }
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

void CommandListGL::SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    GLfloat vp[4] = { static_cast<GLfloat>(x), static_cast<GLfloat>(y),
                      static_cast<GLfloat>(width), static_cast<GLfloat>(height) };
    glViewportIndexedfv(static_cast<GLuint>(index), vp);
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
        glBlendFunc(static_cast<GLenum>(m_blendSrc), static_cast<GLenum>(m_blendDst));
    } else {
        glDisable(GL_BLEND);
    }
}

namespace {
GLenum ToGL(BlendFactor f) {
    switch (f) {
    case BlendFactor::Zero: return GL_ZERO;
    case BlendFactor::One: return GL_ONE;
    case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha: return GL_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::SrcColor: return GL_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstColor: return GL_DST_COLOR;
    case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
    default: return GL_SRC_ALPHA;
    }
}
}

void CommandListGL::SetBlendFunc(BlendFactor src, BlendFactor dst) {
    m_blendSrc = ToGL(src);
    m_blendDst = ToGL(dst);
    if (glIsEnabled(GL_BLEND))
        glBlendFunc(m_blendSrc, m_blendDst);
}

void CommandListGL::SetPolygonOffsetLine(bool enable, float factor, float units) {
    if (enable) {
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(factor, units);
    } else {
        glDisable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(0.f, 0.f);
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

void CommandListGL::MemoryBarrierStorage() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CommandListGL::BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) {
    if (!pboHandle || size == 0) return;
    GLuint id = static_cast<GLuint>(reinterpret_cast<uintptr_t>(pboHandle));
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(slot), id, 0, static_cast<GLsizeiptr>(size));
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
    const bool isCompute = m_currentPipeline->m_vertexLayout.attributes.empty();
    if (isCompute) {
        if (m_boundVAO != 0) {
            glBindVertexArray(0);
            m_boundVAO = 0;
        }
    }
    GLuint program = m_currentPipeline->m_program;
    if (program != m_boundProgram) {
        glUseProgram(program);
        m_boundProgram = program;
    }
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
    m_currentVBO = static_cast<BufferGL*>(vbo);
    m_currentVBOOffset = 0;
    m_currentIBO = static_cast<BufferGL*>(ibo);
    m_currentIndexType = IndexType::UInt16;
    DrawIndexed(6, 0, 0);
}

void CommandListGL::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    (void)binding;
    m_currentVBO = static_cast<BufferGL*>(buffer);
    m_currentVBOOffset = offset;
}

void CommandListGL::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    m_currentIBO = static_cast<BufferGL*>(buffer);
    m_currentIndexType = indexType;
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
    // Some drivers validate stale vertex-input state when rebinding SSBOs between passes.
    // Keep storage binds independent from previous draw VAO state.
    if (m_boundVAO != 0) {
        glBindVertexArray(0);
        m_boundVAO = 0;
    }
    // Ensure no buffer is bound as draw-indirect when we bind as SSBO (same buffer dual-bound can trigger GL 1282).
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, b->m_handle,
                      static_cast<GLintptr>(offset), rangeSize);
}

void CommandListGL::BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) {
    if (!backend::SupportsAtomicCounters(m_device)) return;
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
    const PipelineLayoutGL* layout = ds->layout;
    const bool translate = layout && layout->resourceSetType.has_value();

    for (const auto& t : ds->textures) {
        uint32_t bind = t.binding;
        if (translate)
            bind = GLBindingForResourceSet(*layout->resourceSetType, LookupBindingType(layout, t.binding), t.binding);
        if (t.texture) {
            auto* tex = static_cast<TextureGL*>(t.texture);
            glBindTextureUnit(bind, tex->m_handle);
        } else {
            glBindTextureUnit(bind, 0);
        }
    }
    for (const auto& b : ds->buffers) {
        if (!b.buffer) continue;
        auto* buf = static_cast<BufferGL*>(b.buffer);
        GLsizeiptr sz = b.size > 0 ? static_cast<GLsizeiptr>(b.size) : static_cast<GLsizeiptr>(buf->GetSize());
        uint32_t bind = b.binding;
        if (translate) {
            const BindingType bt = b.isStorage ? BindingType::StorageBuffer : BindingType::UniformBuffer;
            bind = GLBindingForResourceSet(*layout->resourceSetType, bt, b.binding);
        }
        if (b.isStorage)
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bind, buf->m_handle, static_cast<GLintptr>(b.offset), sz);
        else
            glBindBufferRange(GL_UNIFORM_BUFFER, bind, buf->m_handle, static_cast<GLintptr>(b.offset), sz);
    }
}

void CommandListGL::BindDescriptorSets(uint32_t firstSet, uint32_t count, IDescriptorSet* const* sets,
                                       const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
    (void)firstSet;
    (void)dynamicOffsets;
    (void)dynamicOffsetCount;
    if (!sets || count == 0) return;
    for (uint32_t i = 0; i < count; ++i)
        if (sets[i])
            BindDescriptorSet(sets[i]);
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
    if (!backend::SupportsShaderSubroutines(m_device) || !m_currentPipeline || !m_currentPipeline->m_program || !indices) return;
    glUniformSubroutinesuiv(GLEnums::ShaderStageToGL(stage), static_cast<GLsizei>(count), indices);
}

void CommandListGL::SetUniformHandle64(const char* name, uint64_t handle) {
    if (!backend::SupportsBindless(m_device)) return;
    GLint loc = GetUniformLocation(m_currentPipeline, name);
    if (loc >= 0 && GLAD_GL_ARB_bindless_texture)
        glProgramUniformHandleui64ARB(m_currentPipeline->m_program, loc, static_cast<GLuint64>(handle));
}

unsigned int CommandListGL::ResolveAndBindVAO(bool indexed) {
    if (!m_device || !m_currentPipeline) return 0;
    if (m_currentPipeline->m_vertexLayout.attributes.empty()) return 0;  // compute: do not draw
    if (!m_currentVBO) return 0;
    if (indexed && !m_currentIBO) return 0;
    BufferGL* ibo = indexed ? m_currentIBO : nullptr;
    unsigned int vao = m_device->GetOrCreateVAO(m_currentPipeline, m_currentVBO, ibo);
    if (vao == 0) return 0;
    if (vao != m_boundVAO) {
        glBindVertexArray(vao);
        m_boundVAO = vao;
    }
    return vao;
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
    if (!ResolveAndBindVAO(false)) return;
    uint32_t stride = m_currentPipeline->m_vertexLayout.strideInBytes;
    if (stride == 0) stride = 1;
    EYELIB_ASSERT(m_currentVBOOffset % stride == 0);
    GLint baseVertex = static_cast<GLint>(m_currentVBOOffset / stride);
    glDrawArrays(GLEnums::PrimitiveTopologyToGL(m_currentPipeline->m_topology),
                 static_cast<GLint>(firstVertex) + baseVertex, static_cast<GLsizei>(vertexCount));
}

void CommandListGL::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    if (!ResolveAndBindVAO(true)) return;
    uint32_t stride = m_currentPipeline->m_vertexLayout.strideInBytes;
    if (stride == 0) stride = 1;
    EYELIB_ASSERT(m_currentVBOOffset % stride == 0);
    GLint baseVertex = static_cast<GLint>(vertexOffset + static_cast<int32_t>(m_currentVBOOffset / stride));
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    size_t indexSize = (m_currentIndexType == IndexType::UInt16) ? 2 : 4;
    const void* offsetPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex * indexSize));
    glDrawElementsBaseVertex(
        GLEnums::PrimitiveTopologyToGL(m_currentPipeline->m_topology),
        static_cast<GLsizei>(indexCount), idxType, offsetPtr, baseVertex);
}

void CommandListGL::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}

void CommandListGL::DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) {
    if (instanceCount == 0 || !ResolveAndBindVAO(true)) return;
    uint32_t stride = m_currentPipeline->m_vertexLayout.strideInBytes;
    if (stride == 0) stride = 1;
    EYELIB_ASSERT(m_currentVBOOffset % stride == 0);
    GLint baseVertex = static_cast<GLint>(vertexOffset + static_cast<int32_t>(m_currentVBOOffset / stride));
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    size_t indexSize = (m_currentIndexType == IndexType::UInt16) ? 2 : 4;
    const void* offsetPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex * indexSize));
    glDrawElementsInstancedBaseVertex(
        GLEnums::PrimitiveTopologyToGL(m_currentPipeline->m_topology),
        static_cast<GLsizei>(indexCount), idxType, offsetPtr, static_cast<GLsizei>(instanceCount), baseVertex);
}

void CommandListGL::DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) {
    if (!indirectBuffer || !backend::SupportsMultiDrawIndirect(m_device) || !ResolveAndBindVAO(false)) return;
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    glDrawArraysIndirect(mode, reinterpret_cast<const void*>(offset));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) {
    if (!indirectBuffer || !backend::SupportsMultiDrawIndirect(m_device) || !ResolveAndBindVAO(true)) return;
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    glDrawElementsIndirect(mode, idxType, reinterpret_cast<const void*>(offset));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!indirectBuffer || drawCount == 0 || !backend::SupportsMultiDrawIndirect(m_device) || !ResolveAndBindVAO(false)) return;
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    glMultiDrawArraysIndirect(mode, reinterpret_cast<const void*>(offset), static_cast<GLsizei>(drawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!indirectBuffer || drawCount == 0 || !backend::SupportsMultiDrawIndirect(m_device) || !ResolveAndBindVAO(true)) return;
    auto* b = static_cast<BufferGL*>(indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b->m_handle);
    GLenum mode = GLEnums::PrimitiveTopologyToGL(m_currentPipeline ? m_currentPipeline->m_topology : PrimitiveTopology::TriangleList);
    GLenum idxType = GLEnums::IndexTypeToGL(m_currentIndexType);
    glMultiDrawElementsIndirect(mode, idxType, reinterpret_cast<const void*>(offset), static_cast<GLsizei>(drawCount), static_cast<GLsizei>(stride));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void CommandListGL::MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!indirectBuffer || !countBuffer || maxDrawCount == 0 || !backend::SupportsMultiDrawIndirectCount(m_device) || !ResolveAndBindVAO(false)) return;
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
    if (!indirectBuffer || !countBuffer || maxDrawCount == 0 || !backend::SupportsMultiDrawIndirectCount(m_device) || !ResolveAndBindVAO(true)) return;
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
    if (!indirectBuffer || !backend::SupportsCompute(m_device)) return;
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
    if (!backend::SupportsTransformFeedback(m_device)) return;
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
    if (!backend::SupportsTransformFeedback(m_device)) return;
    glEndTransformFeedback();
}

void CommandListGL::BeginConditionalRender(IQueryPool* pool, uint32_t index) {
    if (!backend::SupportsConditionalRender(m_device) || !pool || m_inConditionalRender) return;
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
