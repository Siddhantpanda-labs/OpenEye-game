#include "eyelib/render/rhi/bgfx/CommandListBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/render/rhi/bgfx/PipelineBGFX.h"
#include "eyelib/render/rhi/bgfx/BufferBGFX.h"
#include "eyelib/render/rhi/bgfx/TextureBGFX.h"
#include "eyelib/render/rhi/bgfx/RenderTargetBGFX.h"
#include "eyelib/render/rhi/bgfx/BGFXEnums.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <cstring>

static constexpr uint16_t kInvalidIdx = 0xffff;

namespace eyelib {
namespace render {
namespace rhi {

CommandListBGFX::CommandListBGFX(DeviceBGFX* device) : m_device(device) {}

CommandListBGFX::~CommandListBGFX() {
    if (m_bonesUBO && m_device) {
        m_device->DestroyBuffer(m_bonesUBO);
        m_bonesUBO = nullptr;
        m_bonesUBOSize = 0;
    }
}

void CommandListBGFX::Begin() {
    m_encoder = static_cast<void*>(bgfx::begin());
    m_viewId = 0;
}

void CommandListBGFX::End() {
    m_encoder = nullptr;
}

void CommandListBGFX::SetRenderTarget(IRenderTarget* rt) {
    bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;
    if (rt) {
        auto* r = static_cast<RenderTargetBGFX*>(rt);
        if (r->m_fbIdx != RenderTargetBGFX::kInvalidIdx) fb.idx = r->m_fbIdx;
    }
    bgfx::setViewFrameBuffer(m_viewId, fb);
}

void CommandListBGFX::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListBGFX::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    uint32_t rgba = (uint32_t(clear.color[0] * 255.f) & 0xff)
        | (uint32_t(clear.color[1] * 255.f) & 0xff) << 8
        | (uint32_t(clear.color[2] * 255.f) & 0xff) << 16
        | (uint32_t(clear.color[3] * 255.f) & 0xff) << 24;
    bgfx::setViewClear(m_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, clear.depth, clear.stencil);
    if (rt) {
        bgfx::setViewRect(m_viewId, 0, 0, rt->GetWidth(), rt->GetHeight());
    }
}

void CommandListBGFX::ClearDefaultFramebuffer(const ClearValue& clear) {
    uint32_t rgba = (uint32_t(clear.color[0] * 255.f) & 0xff)
        | (uint32_t(clear.color[1] * 255.f) & 0xff) << 8
        | (uint32_t(clear.color[2] * 255.f) & 0xff) << 16
        | (uint32_t(clear.color[3] * 255.f) & 0xff) << 24;
    bgfx::setViewClear(m_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, clear.depth, clear.stencil);
    bgfx::setViewRect(m_viewId, 0, 0, m_device->GetWidth(), m_device->GetHeight());
    bgfx::touch(m_viewId);
}

void CommandListBGFX::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    m_viewport[0] = x;
    m_viewport[1] = y;
    m_viewport[2] = width;
    m_viewport[3] = height;
    bgfx::setViewRect(m_viewId, (uint16_t)x, (uint16_t)y, (uint16_t)width, (uint16_t)height);
}

void CommandListBGFX::SetDepthTest(bool enable) { m_depthTest = enable; }
void CommandListBGFX::SetDepthWrite(bool enable) { m_depthWrite = enable; }
void CommandListBGFX::SetDepthFuncLess(bool less) { m_depthFuncLess = less; }
void CommandListBGFX::SetCullFace(bool enable) { m_cullFace = enable; }
void CommandListBGFX::SetCullFrontFace(bool cullFront) { m_cullFront = cullFront; }
void CommandListBGFX::SetBlend(bool enable) { m_blend = enable; }

void CommandListBGFX::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    (void)buffer;
    (void)fromState;
    (void)toState;
}

void CommandListBGFX::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    (void)texture;
    (void)fromState;
    (void)toState;
}

void CommandListBGFX::BindPipeline(IPipeline* pipeline) {
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    core::Log::Info("CommandListBGFX::BindPipeline: clearing texture bindings");
    for (int i = 0; i < kMaxTextureBindings; i++) {
        m_boundTextures[i] = nullptr;
        m_samplerNameForUnit[i] = nullptr;
    }
    PipelineBGFX* p = static_cast<PipelineBGFX*>(pipeline);
    m_currentProgramIdx = kInvalidIdx;
    if (!p) return;
    core::Log::InfoF("CommandListBGFX::BindPipeline: programIdx=%u", p->m_programIdx);
    bgfx::ProgramHandle prog; prog.idx = p->m_programIdx;
    if (bgfx::isValid(prog)) {
        m_currentProgramIdx = p->m_programIdx;
        core::Log::Info("CommandListBGFX::BindPipeline: before setState");
        uint64_t state = BGFX_STATE_DEFAULT;
        state |= BGFXEnums::BlendStateToBGFX(p->m_blend);
        state |= BGFXEnums::DepthStencilStateToBGFX(p->m_depthStencil);
        state |= BGFXEnums::RasterStateToBGFX(p->m_raster);
        state |= BGFXEnums::PrimitiveTopologyToBGFX(p->m_topology);
        enc->setState(state);
        core::Log::Info("CommandListBGFX::BindPipeline: after setState");
    }
    core::Log::Info("CommandListBGFX::BindPipeline: done");
}

void CommandListBGFX::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    (void)binding;
    (void)offset;
    m_currentVbIdx = kInvalidIdx;
    if (!buffer) return;
    auto* b = static_cast<BufferBGFX*>(buffer);
    if (!b || !b->m_vertex || b->m_vbIdx == BufferBGFX::kInvalidIdx) return;
    m_currentVbIdx = static_cast<uint16_t>(b->m_vbIdx);
}

void CommandListBGFX::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    (void)indexType;
    (void)offset;
    m_currentIbIdx = kInvalidIdx;
    if (!buffer) return;
    auto* b = static_cast<BufferBGFX*>(buffer);
    if (!b || !b->m_index || b->m_ibIdx == BufferBGFX::kInvalidIdx) return;
    m_currentIbIdx = static_cast<uint16_t>(b->m_ibIdx);
}

void CommandListBGFX::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    (void)offset;
    (void)size;
    // BGFX does not expose binding application buffers as UBO; uniforms are set via setUniform.
    // Bones use the UBO path (create/update buffer, then this call) per RHI standard; when BGFX
    // supports external UBO binding, implement it here.
    if (binding == UniformBinding::Bones && buffer)
        core::Log::InfoF("CommandListBGFX::BindUniformBuffer: bones binding=%u size=%u (no-op on BGFX)", binding, size);
}

void CommandListBGFX::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    (void)buffer;
    (void)binding;
    (void)offset;
    (void)size;
}

void CommandListBGFX::BindTexture(ITexture* texture, uint32_t binding) {
    if (binding < static_cast<uint32_t>(kMaxTextureBindings))
        m_boundTextures[binding] = texture;
}

void CommandListBGFX::UnbindTexture(uint32_t binding) {
    if (binding < static_cast<uint32_t>(kMaxTextureBindings))
        m_boundTextures[binding] = nullptr;
}

void CommandListBGFX::SetPushConstants(uint32_t offset, uint32_t size, const void* data) {
    (void)offset;
    (void)size;
    (void)data;
    // BGFX does not expose push constants; use setUniform for small per-draw data.
}

void CommandListBGFX::SetPackedUniforms() {
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc || m_currentProgramIdx == kInvalidIdx) return;
    uint16_t h = m_device->GetOrCreateUniform("u_TonemapParams", DeviceBGFX::UniformKind::Vec4);
    if (h != kInvalidIdx) { bgfx::UniformHandle uh; uh.idx = h; enc->setUniform(uh, m_tonemapParams, 1); }
    h = m_device->GetOrCreateUniform("u_BloomExtractParams", DeviceBGFX::UniformKind::Vec4);
    if (h != kInvalidIdx) { bgfx::UniformHandle uh; uh.idx = h; enc->setUniform(uh, m_bloomExtractParams, 1); }
    h = m_device->GetOrCreateUniform("u_BlurParams", DeviceBGFX::UniformKind::Vec4);
    if (h != kInvalidIdx) { bgfx::UniformHandle uh; uh.idx = h; enc->setUniform(uh, m_blurParams, 1); }
    h = m_device->GetOrCreateUniform("u_BloomCompositeParams", DeviceBGFX::UniformKind::Vec4);
    if (h != kInvalidIdx) { bgfx::UniformHandle uh; uh.idx = h; enc->setUniform(uh, m_bloomCompositeParams, 1); }
}

void CommandListBGFX::SetUniformInt(const char* name, int value) {
    if (value >= 0 && value < kMaxTextureBindings) {
        // Only store non-empty names so setTexture loop never calls GetOrCreateUniform("")
        if (name && name[0] != '\0')
            m_samplerNameForUnit[value] = name;
        else
            m_samplerNameForUnit[value] = nullptr;
        return;
    }
    float f = static_cast<float>(value);
    SetUniformFloat(name, f);
}

void CommandListBGFX::SetUniformFloat(const char* name, float value) {
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    if (strcmp(name, "u_Exposure") == 0) { m_tonemapParams[0] = value; return; }
    if (strcmp(name, "u_UseACES") == 0) { m_tonemapParams[1] = value ? 1.f : 0.f; return; }
    if (strcmp(name, "u_Threshold") == 0) { m_bloomExtractParams[0] = value; return; }
    if (strcmp(name, "u_Intensity") == 0) { m_blurParams[3] = value; return; }
    if (strcmp(name, "u_Horizontal") == 0) { m_blurParams[2] = value ? 1.f : 0.f; return; }
    if (strcmp(name, "u_BloomIntensity") == 0) { m_bloomCompositeParams[0] = value; return; }
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Vec4);
    if (hidx == kInvalidIdx) return;
    float v[4] = { value, 0.f, 0.f, 0.f };
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, v, 1);
}

void CommandListBGFX::SetUniformVec2(const char* name, const float* data) {
    if (!data) return;
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    if (strcmp(name, "u_Resolution") == 0) { m_blurParams[0] = data[0]; m_blurParams[1] = data[1]; return; }
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Vec4);
    if (hidx == kInvalidIdx) return;
    float v[4] = { data[0], data[1], 0.f, 0.f };
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, v, 1);
}

void CommandListBGFX::SetUniformVec3(const char* name, const float* data) {
    if (!data) return;
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    core::Log::InfoF("CommandListBGFX::SetUniformVec3: name='%s' before GetOrCreateUniform", name ? name : "(null)");
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Vec4);
    if (hidx == kInvalidIdx) return;
    float v[4] = { data[0], data[1], data[2], 0.f };
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, v, 1);
    core::Log::InfoF("CommandListBGFX::SetUniformVec3: name='%s' after setUniform", name ? name : "(null)");
}

void CommandListBGFX::SetUniformVec4(const char* name, const float* data) {
    if (!data) return;
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Vec4);
    if (hidx == kInvalidIdx) return;
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, data, 1);
}

void CommandListBGFX::SetUniformMat4(const char* name, const float* data) {
    if (!data) return;
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Mat4);
    if (hidx == kInvalidIdx) return;
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, data, 1);
}

void CommandListBGFX::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    if (!data || count == 0) return;
    // Standard: bones always in UBO at binding Bones (3). Use buffer path; do not use setUniform
    // (would exceed push constant size on Vulkan).
    if (name && std::strcmp(name, kBonesUniformName) == 0 && m_device) {
        core::Log::InfoF("CommandListBGFX::SetUniformMat4Array: bones UBO path count=%zu (no setUniform)", count);
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
            if (!m_bonesUBO)
                core::Log::Warn("CommandListBGFX::SetUniformMat4Array: failed to create bones UBO");
        }
        if (m_bonesUBO) {
            m_bonesUBO->Update(data, 0, required);
            BindUniformBuffer(m_bonesUBO, UniformBinding::Bones, 0, static_cast<uint32_t>(required));
        }
        return;
    }
    if (count > 8u)
        core::Log::WarnF("CommandListBGFX::SetUniformMat4Array: setUniform path for '%s' count=%zu (large array may exceed Vulkan push constant limit)", name ? name : "(null)", count);
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc) return;
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Mat4);
    if (hidx == kInvalidIdx) return;
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, data, static_cast<uint16_t>(count));
}

void CommandListBGFX::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc || m_currentProgramIdx == kInvalidIdx) return;
    SetPackedUniforms();
    for (int i = 0; i < kMaxTextureBindings; i++) {
        if (!m_boundTextures[i] || !m_samplerNameForUnit[i]) continue;
        TextureBGFX* tex = static_cast<TextureBGFX*>(m_boundTextures[i]);
        if (tex->m_texIdx == TextureBGFX::kInvalidIdx) continue;
        uint16_t uidx = m_device->GetOrCreateUniform(m_samplerNameForUnit[i], DeviceBGFX::UniformKind::Sampler);
        if (uidx != kInvalidIdx) {
            bgfx::UniformHandle uh; uh.idx = uidx;
            bgfx::TextureHandle th; th.idx = tex->m_texIdx;
            enc->setTexture(static_cast<uint8_t>(i), uh, th);
        }
    }
    if (m_currentVbIdx != kInvalidIdx) {
        bgfx::VertexBufferHandle vb; vb.idx = m_currentVbIdx;
        enc->setVertexBuffer(0, vb, firstVertex, vertexCount);
    }
    bgfx::ProgramHandle prog; prog.idx = m_currentProgramIdx;
    enc->submit(m_viewId, prog);
}

void CommandListBGFX::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    (void)vertexOffset;
    bgfx::Encoder* enc = static_cast<bgfx::Encoder*>(m_encoder);
    if (!enc || m_currentProgramIdx == kInvalidIdx) {
        core::Log::Info("CommandListBGFX::DrawIndexed: skipping (no encoder or invalid program)");
        return;
    }
    if (indexCount == 0) {
        core::Log::Info("CommandListBGFX::DrawIndexed: skipping (indexCount=0)");
        return;
    }
    if (m_currentVbIdx == kInvalidIdx || m_currentIbIdx == kInvalidIdx) {
        core::Log::InfoF("CommandListBGFX::DrawIndexed: skipping (invalid VB or IB: vb=%d ib=%d, mesh may be from different backend)",
            m_currentVbIdx == kInvalidIdx ? 0 : 1, m_currentIbIdx == kInvalidIdx ? 0 : 1);
        return;
    }
    core::Log::InfoF("CommandListBGFX::DrawIndexed: before SetPackedUniforms indexCount=%u", indexCount);
    SetPackedUniforms();
    core::Log::Info("CommandListBGFX::DrawIndexed: after SetPackedUniforms, before setTexture");
    for (int i = 0; i < kMaxTextureBindings; i++) {
        if (!m_boundTextures[i] || !m_samplerNameForUnit[i]) continue;
        const char* name = m_samplerNameForUnit[i];
        if (name[0] == '\0') continue;  // skip empty name (should not happen after SetUniformInt guard)
        // BGFX expects valid C-style uniform names (letter/underscore first). Skip invalid to avoid crash.
        char c = name[0];
        if (c != '_' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z')) {
            core::Log::WarnF("CommandListBGFX::DrawIndexed: slot %d skip invalid sampler name (first char 0x%02x)", i, static_cast<unsigned char>(c));
            continue;
        }
        TextureBGFX* tex = static_cast<TextureBGFX*>(m_boundTextures[i]);
        if (!tex || tex->m_texIdx == TextureBGFX::kInvalidIdx) {
            core::Log::InfoF("CommandListBGFX::DrawIndexed: slot %d skip (null tex or invalid texIdx)", i);
            continue;
        }
        core::Log::InfoF("CommandListBGFX::DrawIndexed: setTexture slot=%d name='%s' texIdx=%u", i, name, tex->m_texIdx);
        uint16_t uidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Sampler);
        if (uidx == kInvalidIdx) {
            core::Log::WarnF("CommandListBGFX::DrawIndexed: slot %d GetOrCreateUniform failed for name='%s'", i, name);
            continue;
        }
        bgfx::UniformHandle uh; uh.idx = uidx;
        bgfx::TextureHandle th; th.idx = tex->m_texIdx;
        enc->setTexture(static_cast<uint8_t>(i), uh, th);
        core::Log::InfoF("CommandListBGFX::DrawIndexed: setTexture slot=%d done", i);
    }
    core::Log::Info("CommandListBGFX::DrawIndexed: after setTexture, before setVertexBuffer/setIndexBuffer");
    /* Some drivers/BGFX can choke on very large index counts per draw; cap and skip to avoid crash. */
    constexpr uint32_t kMaxIndexCountPerDraw = 1u << 20;  /* 1M */
    if (indexCount > kMaxIndexCountPerDraw) {
        core::Log::InfoF("CommandListBGFX::DrawIndexed: skipping (indexCount=%u exceeds limit %u)", indexCount, kMaxIndexCountPerDraw);
        return;
    }
    if (indexCount > 65535u)
        core::Log::InfoF("CommandListBGFX::DrawIndexed: submit large draw indexCount=%u firstIndex=%u", indexCount, firstIndex);
    bgfx::VertexBufferHandle vb; vb.idx = m_currentVbIdx;
    enc->setVertexBuffer(0, vb);
    bgfx::IndexBufferHandle ib; ib.idx = m_currentIbIdx;
    enc->setIndexBuffer(ib, firstIndex, indexCount);
    core::Log::Info("CommandListBGFX::DrawIndexed: before enc->submit");
    bgfx::ProgramHandle prog; prog.idx = m_currentProgramIdx;
    enc->submit(m_viewId, prog);
    core::Log::Info("CommandListBGFX::DrawIndexed: after enc->submit");
}

void CommandListBGFX::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}

void CommandListBGFX::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    (void)groupCountX;
    (void)groupCountY;
    (void)groupCountZ;
}

void CommandListBGFX::BeginQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }
void CommandListBGFX::EndQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }

void CommandListBGFX::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) {
    (void)src;
    (void)srcOffset;
    (void)dst;
    (void)dstOffset;
    (void)size;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
