#include "eyelib/render/rhi/diligent/CommandListDiligent.h"
#include "eyelib/render/rhi/diligent/DeviceDiligent.h"
#include "eyelib/render/rhi/diligent/BufferDiligent.h"
#include "eyelib/render/rhi/diligent/PipelineDiligent.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cstring>

#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceVariable.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#endif

namespace eyelib {
namespace render {
namespace rhi {

CommandListDiligent::CommandListDiligent(DeviceDiligent* device) : m_device(device) {}

CommandListDiligent::~CommandListDiligent() {
#if OPENEYE_USE_DILIGENT
    if (m_pConstantsBuffer) { static_cast<Diligent::IBuffer*>(m_pConstantsBuffer)->Release(); m_pConstantsBuffer = nullptr; }
    if (m_pMaterialBuffer) { static_cast<Diligent::IBuffer*>(m_pMaterialBuffer)->Release(); m_pMaterialBuffer = nullptr; }
    if (m_pBonesBuffer) { static_cast<Diligent::IBuffer*>(m_pBonesBuffer)->Release(); m_pBonesBuffer = nullptr; }
#endif
}

void CommandListDiligent::Begin() {
#if OPENEYE_USE_DILIGENT
    m_constantsStaging.resize(kConstantsSize);
    m_materialStaging.resize(kMaterialSize);
    m_bonesStaging.resize(kBonesSize);
    std::memset(m_constantsStaging.data(), 0, kConstantsSize);
    std::memset(m_materialStaging.data(), 0, kMaterialSize);
    std::memset(m_bonesStaging.data(), 0, kBonesSize);
#endif
}
void CommandListDiligent::End() {}

void CommandListDiligent::SetRenderTarget(IRenderTarget* rt) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (ctx) {
        if (!rt) {
            auto* rtv = static_cast<Diligent::ITextureView*>(m_device->GetSwapChainBackBufferRTV());
            if (rtv) ctx->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
#endif
    (void)rt;
}
void CommandListDiligent::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) { (void)rt; (void)colorAttachmentIndex; }
void CommandListDiligent::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) { (void)rt; (void)clear; }

void CommandListDiligent::ClearDefaultFramebuffer(const ClearValue& clear) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    auto* rtv = static_cast<Diligent::ITextureView*>(m_device->GetSwapChainBackBufferRTV());
    if (ctx && rtv) {
        ctx->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->ClearRenderTarget(rtv, clear.color, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
    }
#else
    (void)clear;
#endif
}
void CommandListDiligent::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (ctx) {
        Diligent::Viewport vp;
        vp.MinDepth = 0.f;
        vp.MaxDepth = 1.f;
        vp.TopLeftX = static_cast<float>(x);
        vp.TopLeftY = static_cast<float>(y);
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        ctx->SetViewports(1, &vp, width, height);
    }
#endif
    (void)x; (void)y; (void)width; (void)height;
}
void CommandListDiligent::SetDepthTest(bool enable) { (void)enable; }
void CommandListDiligent::SetDepthWrite(bool enable) { (void)enable; }
void CommandListDiligent::SetDepthFuncLess(bool less) { (void)less; }
void CommandListDiligent::SetCullFace(bool enable) { (void)enable; }
void CommandListDiligent::SetCullFrontFace(bool cullFront) { (void)cullFront; }
void CommandListDiligent::SetBlend(bool enable) { (void)enable; }

void CommandListDiligent::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) { (void)buffer; (void)fromState; (void)toState; }
void CommandListDiligent::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) { (void)texture; (void)fromState; (void)toState; }

void CommandListDiligent::EnsureConstantBuffers() {
#if OPENEYE_USE_DILIGENT
    if (m_pConstantsBuffer) return;
    auto* dev = static_cast<Diligent::IRenderDevice*>(m_device->GetDiligentDevice());
    if (!dev) return;
    using namespace Diligent;
    BufferDesc dgDesc;
    dgDesc.Size = kConstantsSize;
    dgDesc.Usage = USAGE_DYNAMIC;
    dgDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    dgDesc.BindFlags = BIND_UNIFORM_BUFFER;
    dev->CreateBuffer(dgDesc, nullptr, reinterpret_cast<IBuffer**>(&m_pConstantsBuffer));
    dgDesc.Size = kMaterialSize;
    dev->CreateBuffer(dgDesc, nullptr, reinterpret_cast<IBuffer**>(&m_pMaterialBuffer));
    dgDesc.Size = kBonesSize;
    dev->CreateBuffer(dgDesc, nullptr, reinterpret_cast<IBuffer**>(&m_pBonesBuffer));
#endif
}

void CommandListDiligent::CommitUniformsAndResources() {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    auto* pipe = m_currentPipeline ? static_cast<PipelineDiligent*>(m_currentPipeline) : nullptr;
    if (!ctx || !pipe || !pipe->m_pNative || !pipe->m_pSRB) return;
    EnsureConstantBuffers();
    if (!m_pConstantsBuffer || m_constantsStaging.size() < kConstantsSize) return;
    using namespace Diligent;
    auto* dgConstants = reinterpret_cast<Diligent::IBuffer*>(m_pConstantsBuffer);
    auto* dgMaterial = reinterpret_cast<Diligent::IBuffer*>(m_pMaterialBuffer);
    auto* dgBones = reinterpret_cast<Diligent::IBuffer*>(m_pBonesBuffer);
    void* pMapped = nullptr;
    ctx->MapBuffer(dgConstants, MAP_WRITE, MAP_FLAG_DISCARD, pMapped);
    if (pMapped) { std::memcpy(pMapped, m_constantsStaging.data(), kConstantsSize); ctx->UnmapBuffer(dgConstants, MAP_WRITE); }
    pMapped = nullptr;
    ctx->MapBuffer(dgMaterial, MAP_WRITE, MAP_FLAG_DISCARD, pMapped);
    if (pMapped && m_materialStaging.size() >= kMaterialSize) { std::memcpy(pMapped, m_materialStaging.data(), kMaterialSize); ctx->UnmapBuffer(dgMaterial, MAP_WRITE); }
    pMapped = nullptr;
    ctx->MapBuffer(dgBones, MAP_WRITE, MAP_FLAG_DISCARD, pMapped);
    if (pMapped && m_bonesStaging.size() >= kBonesSize) { std::memcpy(pMapped, m_bonesStaging.data(), kBonesSize); ctx->UnmapBuffer(dgBones, MAP_WRITE); }
    auto* srb = static_cast<IShaderResourceBinding*>(pipe->m_pSRB);
    IShaderResourceVariable* vConstants = srb->GetVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (vConstants) vConstants->Set(dgConstants);
    IShaderResourceVariable* vBones = srb->GetVariableByName(SHADER_TYPE_VERTEX, "Bones");
    if (vBones) vBones->Set(dgBones);
    IShaderResourceVariable* vMaterial = srb->GetVariableByName(SHADER_TYPE_PIXEL, "Material");
    if (vMaterial) vMaterial->Set(dgMaterial);
    ctx->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
#endif
}

void CommandListDiligent::BindPipeline(IPipeline* pipeline) {
    m_currentPipeline = pipeline;
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (ctx && pipeline) {
        auto* pipe = static_cast<PipelineDiligent*>(pipeline);
        if (pipe->m_pNative)
            ctx->SetPipelineState(static_cast<Diligent::IPipelineState*>(pipe->m_pNative));
    }
#endif
}
void CommandListDiligent::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    m_currentVertexBuffer = buffer;
    m_currentVertexOffset = offset;
    m_currentVertexBinding = binding;
}
void CommandListDiligent::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    m_currentIndexBuffer = buffer;
    m_currentIndexType = indexType;
    m_currentIndexOffset = offset;
}
void CommandListDiligent::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) { (void)buffer; (void)binding; (void)offset; (void)size; }
void CommandListDiligent::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) { (void)buffer; (void)binding; (void)offset; (void)size; }
void CommandListDiligent::BindTexture(ITexture* texture, uint32_t binding) { (void)texture; (void)binding; }
void CommandListDiligent::UnbindTexture(uint32_t binding) { (void)binding; }

void CommandListDiligent::SetPushConstants(uint32_t offset, uint32_t size, const void* data) { (void)offset; (void)size; (void)data; }

// Constants cbuffer: u_Model 0, u_View 64, u_Projection 128, u_NormalMatrix 192 (row_major = 64 bytes each)
static constexpr size_t kOff_u_Model = 0;
static constexpr size_t kOff_u_View = 64;
static constexpr size_t kOff_u_Projection = 128;
static constexpr size_t kOff_u_NormalMatrix = 192;
// Material cbuffer std140: u_Albedo 0, u_Metallic 16, u_Roughness 20, u_Emission 32, u_Alpha 48, u_LightDir 64, u_LightIntensity 80, u_LightColor 96, u_Exposure 112, u_ViewPos 128, u_Ambient 160, u_ShadowBias 176, u_UseShadows 192
static constexpr size_t kOff_u_Albedo = 0;
static constexpr size_t kOff_u_Metallic = 16;
static constexpr size_t kOff_u_Roughness = 20;
static constexpr size_t kOff_u_Emission = 32;
static constexpr size_t kOff_u_Alpha = 48;
static constexpr size_t kOff_u_LightDir = 64;
static constexpr size_t kOff_u_LightIntensity = 80;
static constexpr size_t kOff_u_LightColor = 96;
static constexpr size_t kOff_u_Exposure = 112;
static constexpr size_t kOff_u_ViewPos = 128;
static constexpr size_t kOff_u_Ambient = 160;
static constexpr size_t kOff_u_ShadowBias = 176;
static constexpr size_t kOff_u_UseShadows = 192;

void CommandListDiligent::SetUniformInt(const char* name, int value) {
    if (!name || m_materialStaging.size() < kOff_u_UseShadows + 4) return;
    if (std::strcmp(name, "u_UseShadows") == 0) std::memcpy(m_materialStaging.data() + kOff_u_UseShadows, &value, 4);
}
void CommandListDiligent::SetUniformFloat(const char* name, float value) {
    if (!name || m_materialStaging.size() < kOff_u_ShadowBias + 4) return;
    if (std::strcmp(name, "u_Metallic") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Metallic, &value, 4);
    else if (std::strcmp(name, "u_Roughness") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Roughness, &value, 4);
    else if (std::strcmp(name, "u_Alpha") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Alpha, &value, 4);
    else if (std::strcmp(name, "u_LightIntensity") == 0) std::memcpy(m_materialStaging.data() + kOff_u_LightIntensity, &value, 4);
    else if (std::strcmp(name, "u_Exposure") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Exposure, &value, 4);
    else if (std::strcmp(name, "u_ShadowBias") == 0) std::memcpy(m_materialStaging.data() + kOff_u_ShadowBias, &value, 4);
}
void CommandListDiligent::SetUniformVec2(const char* name, const float* data) { (void)name; (void)data; }
void CommandListDiligent::SetUniformVec3(const char* name, const float* data) {
    if (!name || !data || m_materialStaging.size() < kOff_u_Ambient + 12) return;
    if (std::strcmp(name, "u_Albedo") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Albedo, data, 12);
    else if (std::strcmp(name, "u_Emission") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Emission, data, 12);
    else if (std::strcmp(name, "u_LightDir") == 0) std::memcpy(m_materialStaging.data() + kOff_u_LightDir, data, 12);
    else if (std::strcmp(name, "u_LightColor") == 0) std::memcpy(m_materialStaging.data() + kOff_u_LightColor, data, 12);
    else if (std::strcmp(name, "u_ViewPos") == 0) std::memcpy(m_materialStaging.data() + kOff_u_ViewPos, data, 12);
    else if (std::strcmp(name, "u_Ambient") == 0) std::memcpy(m_materialStaging.data() + kOff_u_Ambient, data, 12);
}
void CommandListDiligent::SetUniformVec4(const char* name, const float* data) { (void)name; (void)data; }
void CommandListDiligent::SetUniformMat4(const char* name, const float* data) {
    if (!name || !data || m_constantsStaging.size() < kOff_u_NormalMatrix + 64) return;
    if (std::strcmp(name, "u_Model") == 0) std::memcpy(m_constantsStaging.data() + kOff_u_Model, data, 64);
    else if (std::strcmp(name, "u_View") == 0) std::memcpy(m_constantsStaging.data() + kOff_u_View, data, 64);
    else if (std::strcmp(name, "u_Projection") == 0) std::memcpy(m_constantsStaging.data() + kOff_u_Projection, data, 64);
    else if (std::strcmp(name, "u_NormalMatrix") == 0) std::memcpy(m_constantsStaging.data() + kOff_u_NormalMatrix, data, 64);
}
void CommandListDiligent::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    if (!name || std::strcmp(name, "u_Bones") != 0 || !data || count == 0 || m_bonesStaging.size() < count * 64) return;
    std::memcpy(m_bonesStaging.data(), data, count * 64);
}

void CommandListDiligent::Draw(uint32_t vertexCount, uint32_t firstVertex) { (void)vertexCount; (void)firstVertex; }

void CommandListDiligent::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (!ctx || !m_currentPipeline) return;
    CommitUniformsAndResources();
    auto* pipe = static_cast<PipelineDiligent*>(m_currentPipeline);
    Diligent::IBuffer* pVB = nullptr;
    if (m_currentVertexBuffer) {
        auto* vb = static_cast<BufferDiligent*>(m_currentVertexBuffer);
        if (vb->m_pNative) pVB = static_cast<Diligent::IBuffer*>(vb->m_pNative);
    }
    if (pVB) {
        Diligent::Uint64 offset = m_currentVertexOffset;
        ctx->SetVertexBuffers(0, 1, &pVB, &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    }
    Diligent::IBuffer* pIB = nullptr;
    if (m_currentIndexBuffer) {
        auto* ib = static_cast<BufferDiligent*>(m_currentIndexBuffer);
        if (ib->m_pNative) pIB = static_cast<Diligent::IBuffer*>(ib->m_pNative);
    }
    if (pIB) ctx->SetIndexBuffer(pIB, m_currentIndexOffset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::DrawIndexedAttribs attrs;
    attrs.NumIndices = indexCount;
    attrs.FirstIndexLocation = firstIndex;
    attrs.BaseVertex = vertexOffset;
    attrs.IndexType = (m_currentIndexType == IndexType::UInt16) ? Diligent::VT_UINT16 : Diligent::VT_UINT32;
    ctx->DrawIndexed(attrs);
#endif
    (void)indexCount; (void)firstIndex; (void)vertexOffset;
}

void CommandListDiligent::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}
void CommandListDiligent::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) { (void)groupCountX; (void)groupCountY; (void)groupCountZ; }

void CommandListDiligent::BeginQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }
void CommandListDiligent::EndQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }

void CommandListDiligent::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) { (void)src; (void)srcOffset; (void)dst; (void)dstOffset; (void)size; }

} // namespace rhi
} // namespace render
} // namespace eyelib
