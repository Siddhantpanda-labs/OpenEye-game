#include "eyelib/render/rhi/diligent/CommandListDiligent.h"
#include "eyelib/render/rhi/diligent/DeviceDiligent.h"
#include "eyelib/render/rhi/diligent/PipelineDiligent.h"
#include "eyelib/render/rhi/diligent/BufferDiligent.h"
#include "eyelib/render/rhi/diligent/RenderTargetDiligent.h"
#include "eyelib/render/rhi/diligent/TextureDiligent.h"
#include "eyelib/render/rhi/RHITypes.h"

#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceVariable.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include <cstring>
#endif

namespace eyelib {
namespace render {
namespace rhi {

CommandListDiligent::CommandListDiligent(DeviceDiligent* device) : m_device(device) {}

CommandListDiligent::~CommandListDiligent() {
#if OPENEYE_USE_DILIGENT
    for (void* p : {m_cbConstants, m_cbMaterial, m_cbBones}) {
        if (p) static_cast<Diligent::IBuffer*>(p)->Release();
    }
    m_cbConstants = m_cbMaterial = m_cbBones = nullptr;
#endif
}

#if OPENEYE_USE_DILIGENT
void CommandListDiligent::EnsureDynamicConstantBuffers() {
    auto* pDevice = static_cast<Diligent::IRenderDevice*>(m_device->GetRenderDevice());
    if (!pDevice || m_cbConstants) return;
    Diligent::BufferDesc dgDesc;
    dgDesc.Size = static_cast<Diligent::Uint64>(kConstantsSize);
    dgDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    dgDesc.Usage = Diligent::USAGE_DYNAMIC;
    dgDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    dgDesc.Mode = Diligent::BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(dgDesc, nullptr, reinterpret_cast<Diligent::IBuffer**>(&m_cbConstants));
    dgDesc.Size = static_cast<Diligent::Uint64>(kMaterialSize);
    pDevice->CreateBuffer(dgDesc, nullptr, reinterpret_cast<Diligent::IBuffer**>(&m_cbMaterial));
    dgDesc.Size = static_cast<Diligent::Uint64>(kBonesSize);
    pDevice->CreateBuffer(dgDesc, nullptr, reinterpret_cast<Diligent::IBuffer**>(&m_cbBones));
}

void CommandListDiligent::CommitShaderResources() {
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (!ctx || !m_currentPipeline || !m_currentPipeline->m_pSRB) return;
    Diligent::IShaderResourceBinding* pSRB = static_cast<Diligent::IShaderResourceBinding*>(m_currentPipeline->m_pSRB);
    EnsureDynamicConstantBuffers();
    auto* pCbConstants = static_cast<Diligent::IBuffer*>(m_cbConstants);
    auto* pCbMaterial = static_cast<Diligent::IBuffer*>(m_cbMaterial);
    auto* pCbBones = static_cast<Diligent::IBuffer*>(m_cbBones);
    if (m_constantsDirty && pCbConstants) {
        void* pData = nullptr;
        ctx->MapBuffer(pCbConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, pData);
        if (pData) memcpy(pData, m_constantsStaging, kConstantsSize);
        ctx->UnmapBuffer(pCbConstants, Diligent::MAP_WRITE);
        m_constantsDirty = false;
    }
    if (m_materialDirty && pCbMaterial) {
        void* pData = nullptr;
        ctx->MapBuffer(pCbMaterial, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, pData);
        if (pData) memcpy(pData, m_materialStaging, kMaterialSize);
        ctx->UnmapBuffer(pCbMaterial, Diligent::MAP_WRITE);
        m_materialDirty = false;
    }
    if (m_bonesDirty && pCbBones) {
        void* pData = nullptr;
        ctx->MapBuffer(pCbBones, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, pData);
        if (pData) memcpy(pData, m_bonesStaging, kBonesSize);
        ctx->UnmapBuffer(pCbBones, Diligent::MAP_WRITE);
        m_bonesDirty = false;
    }
    auto setVar = [pSRB](Diligent::SHADER_TYPE st, const char* name, Diligent::IDeviceObject* pObj) {
        if (auto* v = pSRB->GetVariableByName(st, name))
            v->Set(pObj);
    };
    if (pCbConstants) {
        setVar(Diligent::SHADER_TYPE_VERTEX, "Constants", pCbConstants);
        setVar(Diligent::SHADER_TYPE_PIXEL, "Constants", pCbConstants);
    }
    if (pCbMaterial) setVar(Diligent::SHADER_TYPE_PIXEL, "Material", pCbMaterial);
    if (pCbBones) setVar(Diligent::SHADER_TYPE_VERTEX, "Bones", pCbBones);
    ctx->CommitShaderResources(pSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
#else
void CommandListDiligent::EnsureDynamicConstantBuffers() {}
void CommandListDiligent::CommitShaderResources() {}
#endif

void CommandListDiligent::Begin() {}
void CommandListDiligent::End() {}

void CommandListDiligent::SetRenderTarget(IRenderTarget* rt) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (!ctx) return;
    if (!rt) {
        auto* rtv = static_cast<Diligent::ITextureView*>(m_device->GetSwapChainBackBufferRTV());
        if (rtv)
            ctx->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        return;
    }
    auto* rtd = static_cast<RenderTargetDiligent*>(rt);
    ITexture* color0 = rtd->GetColorTexture(0);
    if (!color0) return;
    TextureDiligent* colorTex = static_cast<TextureDiligent*>(color0);
    if (!colorTex->m_pRTV) return;
    Diligent::ITextureView* pRTV = static_cast<Diligent::ITextureView*>(colorTex->m_pRTV);
    Diligent::ITextureView* pDSV = nullptr;
    if (rtd->m_depthTexture && rtd->m_depthTexture->m_pDSV)
        pDSV = static_cast<Diligent::ITextureView*>(rtd->m_depthTexture->m_pDSV);
    ctx->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
#endif
}

void CommandListDiligent::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListDiligent::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
#if OPENEYE_USE_DILIGENT
    (void)rt;
    (void)clear;
    // TODO: clear when RT has native RTV
#endif
}

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
        vp.TopLeftX = static_cast<float>(x);
        vp.TopLeftY = static_cast<float>(y);
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.f;
        vp.MaxDepth = 1.f;
        ctx->SetViewports(1, &vp, width, height);
    }
#endif
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
void CommandListDiligent::SetDepthTest(bool enable) { (void)enable; }
void CommandListDiligent::SetDepthWrite(bool enable) { (void)enable; }
void CommandListDiligent::SetDepthFuncLess(bool less) { (void)less; }
void CommandListDiligent::SetCullFace(bool enable) { (void)enable; }
void CommandListDiligent::SetCullFrontFace(bool cullFront) { (void)cullFront; }
void CommandListDiligent::SetBlend(bool enable) { (void)enable; }

void CommandListDiligent::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) { (void)buffer; (void)fromState; (void)toState; }
void CommandListDiligent::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) { (void)texture; (void)fromState; (void)toState; }

void CommandListDiligent::BindPipeline(IPipeline* pipeline) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    m_currentPipeline = pipeline ? static_cast<PipelineDiligent*>(pipeline) : nullptr;
    if (ctx && m_currentPipeline && m_currentPipeline->m_pNative)
        ctx->SetPipelineState(static_cast<Diligent::IPipelineState*>(m_currentPipeline->m_pNative));
#endif
}

void CommandListDiligent::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    (void)binding;
    m_currentVertexBuffer = buffer;
    m_currentVertexOffset = offset;
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

void CommandListDiligent::SetUniformInt(const char* name, int value) {
#if OPENEYE_USE_DILIGENT
    if (!name || !m_constantsStaging) return;
    if (strcmp(name, "u_UseShadows") == 0) {
        *reinterpret_cast<int*>(m_materialStaging + 192) = value;
        m_materialDirty = true;
    }
#endif
    (void)value;
}

void CommandListDiligent::SetUniformFloat(const char* name, float value) {
#if OPENEYE_USE_DILIGENT
    if (!name) return;
    auto* m = reinterpret_cast<float*>(m_materialStaging);
    if (strcmp(name, "u_Metallic") == 0) { m[4] = value; m_materialDirty = true; return; }
    if (strcmp(name, "u_Roughness") == 0) { m[5] = value; m_materialDirty = true; return; }
    if (strcmp(name, "u_Alpha") == 0) { m[13] = value; m_materialDirty = true; return; }
    if (strcmp(name, "u_LightIntensity") == 0) { m[21] = value; m_materialDirty = true; return; }
    if (strcmp(name, "u_Exposure") == 0) { m[29] = value; m_materialDirty = true; return; }
    if (strcmp(name, "u_ShadowBias") == 0) { m[45] = value; m_materialDirty = true; return; }
#endif
    (void)value;
}

void CommandListDiligent::SetUniformVec2(const char* name, const float* data) { (void)name; (void)data; }

void CommandListDiligent::SetUniformVec3(const char* name, const float* data) {
#if OPENEYE_USE_DILIGENT
    if (!name || !data) return;
    auto* m = reinterpret_cast<float*>(m_materialStaging);
    if (strcmp(name, "u_Albedo") == 0) { m[0] = data[0]; m[1] = data[1]; m[2] = data[2]; m_materialDirty = true; return; }
    if (strcmp(name, "u_Emission") == 0) { m[8] = data[0]; m[9] = data[1]; m[10] = data[2]; m_materialDirty = true; return; }
    if (strcmp(name, "u_LightDir") == 0) { m[16] = data[0]; m[17] = data[1]; m[18] = data[2]; m_materialDirty = true; return; }
    if (strcmp(name, "u_LightColor") == 0) { m[24] = data[0]; m[25] = data[1]; m[26] = data[2]; m_materialDirty = true; return; }
    if (strcmp(name, "u_ViewPos") == 0) { m[32] = data[0]; m[33] = data[1]; m[34] = data[2]; m_materialDirty = true; return; }
    if (strcmp(name, "u_Ambient") == 0) { m[40] = data[0]; m[41] = data[1]; m[42] = data[2]; m_materialDirty = true; return; }
#endif
}

void CommandListDiligent::SetUniformVec4(const char* name, const float* data) {
#if OPENEYE_USE_DILIGENT
    if (!name || !data) return;
    if (strcmp(name, "u_UVTransform") == 0) {
        memcpy(m_constantsStaging + 256, data, 16);
        m_constantsDirty = true;
    }
#endif
}

void CommandListDiligent::SetUniformMat4(const char* name, const float* data) {
#if OPENEYE_USE_DILIGENT
    if (!name || !data) return;
    const size_t matSize = 64u;
    if (strcmp(name, "u_Model") == 0) { memcpy(m_constantsStaging + 0, data, matSize); m_constantsDirty = true; return; }
    if (strcmp(name, "u_View") == 0) { memcpy(m_constantsStaging + 64, data, matSize); m_constantsDirty = true; return; }
    if (strcmp(name, "u_Projection") == 0) { memcpy(m_constantsStaging + 128, data, matSize); m_constantsDirty = true; return; }
    if (strcmp(name, "u_NormalMatrix") == 0) { memcpy(m_constantsStaging + 192, data, matSize); m_constantsDirty = true; return; }
#endif
}

void CommandListDiligent::SetUniformMat4Array(const char* name, const float* data, size_t count) {
#if OPENEYE_USE_DILIGENT
    if (strcmp(name, "u_Bones") != 0 || !data || count == 0 || count > 128) return;
    memcpy(m_bonesStaging, data, count * 64u);
    m_bonesDirty = true;
#endif
    (void)count;
}

void CommandListDiligent::Draw(uint32_t vertexCount, uint32_t firstVertex) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (!ctx || !m_currentPipeline || !m_currentPipeline->m_pNative) return;
    CommitShaderResources();
    if (m_currentVertexBuffer) {
        auto* b = static_cast<BufferDiligent*>(m_currentVertexBuffer);
        if (b->m_pNative) {
            Diligent::IBuffer* pVB = static_cast<Diligent::IBuffer*>(b->m_pNative);
            Diligent::Uint64 offset = m_currentVertexOffset;
            ctx->SetVertexBuffers(0, 1, &pVB, &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
    ctx->Draw(Diligent::DrawAttribs(vertexCount, Diligent::DRAW_FLAG_NONE, 1, firstVertex, 0));
#endif
    (void)vertexCount;
    (void)firstVertex;
}

void CommandListDiligent::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
#if OPENEYE_USE_DILIGENT
    auto* ctx = static_cast<Diligent::IDeviceContext*>(m_device->GetImmediateContext());
    if (!ctx || !m_currentPipeline || !m_currentPipeline->m_pNative) return;
    CommitShaderResources();
    if (m_currentVertexBuffer) {
        auto* vb = static_cast<BufferDiligent*>(m_currentVertexBuffer);
        if (vb->m_pNative) {
            Diligent::IBuffer* pVB = static_cast<Diligent::IBuffer*>(vb->m_pNative);
            Diligent::Uint64 voff = m_currentVertexOffset;
            ctx->SetVertexBuffers(0, 1, &pVB, &voff, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
    if (m_currentIndexBuffer) {
        auto* ib = static_cast<BufferDiligent*>(m_currentIndexBuffer);
        if (ib->m_pNative) {
            const Diligent::VALUE_TYPE vt = (m_currentIndexType == IndexType::UInt32) ? Diligent::VT_UINT32 : Diligent::VT_UINT16;
            ctx->SetIndexBuffer(static_cast<Diligent::IBuffer*>(ib->m_pNative), m_currentIndexOffset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            Diligent::DrawIndexedAttribs attr;
            attr.NumIndices = indexCount;
            attr.IndexType = vt;
            attr.FirstIndexLocation = firstIndex;
            attr.BaseVertex = vertexOffset;
            ctx->DrawIndexed(attr);
        }
    }
#endif
    (void)indexCount;
    (void)firstIndex;
    (void)vertexOffset;
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
