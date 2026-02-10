#include "eyelib/render/rhi/bgfx/CommandListBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/render/rhi/bgfx/PipelineBGFX.h"
#include "eyelib/render/rhi/bgfx/BufferBGFX.h"
#include "eyelib/render/rhi/bgfx/TextureBGFX.h"
#include "eyelib/render/rhi/bgfx/RenderTargetBGFX.h"
#include "eyelib/render/rhi/bgfx/BGFXEnums.h"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <cstring>

static constexpr uint16_t kInvalidIdx = 0xffff;

namespace eyelib {
namespace render {
namespace rhi {

CommandListBGFX::CommandListBGFX(DeviceBGFX* device) : m_device(device) {}

CommandListBGFX::~CommandListBGFX() = default;

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
    for (int i = 0; i < kMaxTextureBindings; i++) {
        m_boundTextures[i] = nullptr;
        m_samplerNameForUnit[i] = nullptr;
    }
    PipelineBGFX* p = static_cast<PipelineBGFX*>(pipeline);
    m_currentProgramIdx = kInvalidIdx;
    if (!p) return;
    bgfx::ProgramHandle prog; prog.idx = p->m_programIdx;
    if (bgfx::isValid(prog)) {
        m_currentProgramIdx = p->m_programIdx;
        uint64_t state = BGFX_STATE_DEFAULT;
        state |= BGFXEnums::BlendStateToBGFX(p->m_blend);
        state |= BGFXEnums::DepthStencilStateToBGFX(p->m_depthStencil);
        state |= BGFXEnums::RasterStateToBGFX(p->m_raster);
        state |= BGFXEnums::PrimitiveTopologyToBGFX(p->m_topology);
        enc->setState(state);
    }
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
    (void)buffer;
    (void)binding;
    (void)offset;
    (void)size;
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
        m_samplerNameForUnit[value] = name;
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
    uint16_t hidx = m_device->GetOrCreateUniform(name, DeviceBGFX::UniformKind::Vec4);
    if (hidx == kInvalidIdx) return;
    float v[4] = { data[0], data[1], data[2], 0.f };
    bgfx::UniformHandle uh; uh.idx = hidx;
    enc->setUniform(uh, v, 1);
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
        enc->setVertexBuffer(0, vb);
    }
    if (m_currentIbIdx != kInvalidIdx) {
        bgfx::IndexBufferHandle ib; ib.idx = m_currentIbIdx;
        enc->setIndexBuffer(ib, firstIndex, indexCount);
    }
    bgfx::ProgramHandle prog; prog.idx = m_currentProgramIdx;
    enc->submit(m_viewId, prog);
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
