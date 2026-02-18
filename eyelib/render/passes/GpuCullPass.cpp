#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/core/GLDrawDebugLabel.h"
#include "eyelib/core/Log.h"
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr uint32_t kMaxIndirectDraws = 256;
constexpr uint32_t kIndirectRegionSize = kMaxIndirectDraws * static_cast<uint32_t>(sizeof(rhi::DrawIndexedIndirectCommand));
constexpr uint32_t kDrawListRegionSize = kMaxIndirectDraws * static_cast<uint32_t>(sizeof(GpuCullDrawEntry));
constexpr const char* kCullCompPath = "assets/shaders/opengl/passes/gpu_cull/cull_indirect.comp";
} // namespace

GpuCullPass::~GpuCullPass() {
    Shutdown();
}

bool GpuCullPass::IsSupported() const {
    if (!m_device) return false;
    const auto& caps = m_device->GetCapabilities();
    return caps.supportsCompute && caps.supportsMultiDrawIndirect;
}

void GpuCullPass::EnsureResources() {
    if (!m_device || m_initialized || !IsSupported()) return;

    const size_t combinedSize = kIndirectRegionSize + kDrawListRegionSize;
    rhi::BufferDesc bd;
    bd.sizeInBytes = combinedSize;
    bd.usage = rhi::BufferUsage::Storage | rhi::BufferUsage::IndirectDraw;
    bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_indirectBuffer = m_device->CreateBuffer(bd, nullptr);
    if (!m_indirectBuffer) {
        core::Log::Error("GpuCullPass: failed to create indirect buffer");
        return;
    }

    std::string compSrc;
    if (!LoadShaderSource(kCullCompPath, compSrc)) {
        core::Log::ErrorF("GpuCullPass: failed to load %s", kCullCompPath);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
        return;
    }
    m_computeShader = m_device->CreateShader(rhi::ShaderStage::Compute, compSrc.c_str(), compSrc.size());
    if (!m_computeShader || !m_computeShader->IsValid()) {
        core::Log::Error("GpuCullPass: failed to create compute shader");
        if (m_computeShader) m_device->DestroyShader(m_computeShader);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
        return;
    }
    m_computePipeline = m_device->CreateComputePipeline(m_computeShader);
    if (!m_computePipeline) {
        core::Log::Error("GpuCullPass: failed to create compute pipeline");
        m_device->DestroyShader(m_computeShader);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShader = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    rhi::BufferDesc uboDesc;
    uboDesc.sizeInBytes = 16 * sizeof(float);
    uboDesc.usage = rhi::BufferUsage::Uniform;
    uboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_viewProjUBO = m_device->CreateBuffer(uboDesc, nullptr);
    if (!m_viewProjUBO) {
        core::Log::Error("GpuCullPass: failed to create view-proj UBO");
        m_device->DestroyPipeline(m_computePipeline);
        m_device->DestroyShader(m_computeShader);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computePipeline = nullptr;
        m_computeShader = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    m_initialized = true;
}

bool GpuCullPass::Initialize() {
    Shutdown();
    EnsureResources();
    return m_initialized;
}

void GpuCullPass::Shutdown() {
    if (!m_device) return;
    if (m_computePipeline) {
        m_device->DestroyPipeline(m_computePipeline);
        m_computePipeline = nullptr;
    }
    if (m_computeShader) {
        m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
    }
    if (m_indirectBuffer) {
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
    }
    if (m_viewProjUBO) {
        m_device->DestroyBuffer(m_viewProjUBO);
        m_viewProjUBO = nullptr;
    }
    m_initialized = false;
}

void GpuCullPass::SetDrawList(const GpuCullDrawEntry* entries, uint32_t numDraws) {
    if (entries && numDraws > 0 && m_indirectBuffer) {
        const size_t uploadSize = numDraws * sizeof(GpuCullDrawEntry);
        m_indirectBuffer->Update(entries, kIndirectRegionSize, uploadSize);
        m_drawCount = numDraws;
    } else {
        m_drawCount = 0;  // legacy path: no slot 1 bind, dispatch with u_NumDraws=0
    }
}

void GpuCullPass::SetViewProj(const float* viewProj16) {
    if (viewProj16) {
        for (int i = 0; i < 16; ++i) m_viewProj[i] = viewProj16[i];
        if (m_viewProjUBO)
            m_viewProjUBO->Update(m_viewProj, 0, 16 * sizeof(float));
    }
}

void GpuCullPass::SetOcclusionDepth(rhi::ITexture* depthTexture, uint32_t width, uint32_t height) {
    m_occlusionDepthTex = depthTexture;
    m_occlusionDepthWidth = width;
    m_occlusionDepthHeight = height;
}

void GpuCullPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !IsSupported()) return;
    EnsureResources();
    if (!m_computePipeline || !m_indirectBuffer) return;

    const bool useDrawList = (m_drawCount > 0);
    const uint32_t effectiveCount = useDrawList ? (m_drawCount <= kMaxIndirectDraws ? m_drawCount : kMaxIndirectDraws) : 0u;
    m_effectiveDrawCount = (useDrawList && effectiveCount > 0) ? effectiveCount : 1u;

    const uint32_t groupSize = 64u;
    const uint32_t dispatchX = (useDrawList && effectiveCount > 0) ? ((effectiveCount + groupSize - 1u) / groupSize) : 1u;
    const uint32_t dispatchY = 1u;
    const uint32_t dispatchZ = 1u;

    core::SetGLDrawDebugLabel("GpuCull Bind");
    cmd->BindPipeline(m_computePipeline);
    if (m_device)
        m_device->EnsureComputePipelineStorageBlockBindings(m_computePipeline);
    core::SetGLDrawDebugLabel("GpuCull BindStorage slot0");
    cmd->BindStorageBuffer(m_indirectBuffer, 0, 0, kIndirectRegionSize);
    core::SetGLDrawDebugLabel("GpuCull AfterBind0");
    if (m_viewProjUBO)
        cmd->BindUniformBuffer(m_viewProjUBO, 3, 0, static_cast<uint32_t>(16 * sizeof(float)));
    core::SetGLDrawDebugLabel("GpuCull AfterUBO");

    const bool hasDrawListBlock = m_device ? m_device->ComputePipelineHasStorageBlock(m_computePipeline, "DrawListIn") : false;
    if (hasDrawListBlock && useDrawList && effectiveCount > 0) {
        core::SetGLDrawDebugLabel("GpuCull BindStorage slot1");
        const uint32_t drawListBytes = effectiveCount * static_cast<uint32_t>(sizeof(GpuCullDrawEntry));
        cmd->BindStorageBuffer(m_indirectBuffer, 1, kIndirectRegionSize, drawListBytes);
        core::SetGLDrawDebugLabel("GpuCull AfterBind1");
        cmd->SetUniformUint("u_NumDraws", effectiveCount);
        core::SetGLDrawDebugLabel("GpuCull AfterSetNumDraws");
        const bool useOcclusion = (m_occlusionDepthTex != nullptr && m_occlusionDepthWidth > 0 && m_occlusionDepthHeight > 0);
        cmd->SetUniformInt("u_UseOcclusionCull", useOcclusion ? 1 : 0);
        core::SetGLDrawDebugLabel("GpuCull AfterSetOcclusion");
        if (useOcclusion) {
            cmd->BindTexture(m_occlusionDepthTex, 2);
            float depthSize[2] = { static_cast<float>(m_occlusionDepthWidth), static_cast<float>(m_occlusionDepthHeight) };
            cmd->SetUniformVec2("u_DepthSize", depthSize);
        }
        core::SetGLDrawDebugLabel("GpuCull BeforeDispatch");
        cmd->Dispatch(dispatchX, dispatchY, dispatchZ);
        core::SetGLDrawDebugLabel("GpuCull AfterDispatch");
    } else {
        cmd->SetUniformInt("u_NumDraws", 0);
        core::SetGLDrawDebugLabel("GpuCull Dispatch");
        cmd->Dispatch(dispatchX, dispatchY, dispatchZ);
    }

    core::SetGLDrawDebugLabel("GpuCull Barrier");
    rhi::BarrierBufferForIndirectDraw(cmd, m_indirectBuffer);
}

} // namespace render
} // namespace eyelib
