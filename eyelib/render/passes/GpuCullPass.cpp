#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/core/Log.h"
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr uint32_t kMaxIndirectDraws = 256;
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

    const size_t bufferSize = kMaxIndirectDraws * sizeof(rhi::DrawIndexedIndirectCommand);
    rhi::BufferDesc bd;
    bd.sizeInBytes = bufferSize;
    bd.usage = rhi::BufferUsage::Storage | rhi::BufferUsage::IndirectDraw;
    bd.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
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
    m_drawListBuffer = nullptr;
    m_initialized = false;
}

void GpuCullPass::SetDrawList(rhi::IBuffer* buffer, uint32_t numDraws) {
    m_drawListBuffer = buffer;
    m_drawCount = (buffer && numDraws > 0) ? numDraws : 1u;
}

void GpuCullPass::SetViewProj(const float* viewProj16) {
    if (viewProj16) {
        for (int i = 0; i < 16; ++i) m_viewProj[i] = viewProj16[i];
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

    cmd->BindPipeline(m_computePipeline);
    cmd->BindStorageBuffer(m_indirectBuffer, 0, 0, 0);

    const bool useDrawList = (m_drawListBuffer != nullptr && m_drawCount > 0);
    const uint32_t effectiveCount = useDrawList ? (m_drawCount <= kMaxIndirectDraws ? m_drawCount : kMaxIndirectDraws) : 0u;
    m_effectiveDrawCount = (useDrawList && effectiveCount > 0) ? effectiveCount : 1u;

    if (useDrawList && effectiveCount > 0) {
        cmd->BindStorageBuffer(m_drawListBuffer, 1, 0, 0);
        cmd->SetUniformMat4("u_ViewProj", m_viewProj);
        cmd->SetUniformInt("u_NumDraws", static_cast<int>(effectiveCount));
        const bool useOcclusion = (m_occlusionDepthTex != nullptr && m_occlusionDepthWidth > 0 && m_occlusionDepthHeight > 0);
        cmd->SetUniformInt("u_UseOcclusionCull", useOcclusion ? 1 : 0);
        if (useOcclusion) {
            cmd->BindTexture(m_occlusionDepthTex, 2);
            float depthSize[2] = { static_cast<float>(m_occlusionDepthWidth), static_cast<float>(m_occlusionDepthHeight) };
            cmd->SetUniformVec2("u_DepthSize", depthSize);
        }
        const uint32_t groupSize = 64u;
        const uint32_t groups = (effectiveCount + groupSize - 1u) / groupSize;
        cmd->Dispatch(groups, 1, 1);
    } else {
        cmd->SetUniformInt("u_NumDraws", 0);
        cmd->Dispatch(1, 1, 1);
    }

    rhi::BarrierBufferForIndirectDraw(cmd, m_indirectBuffer);
}

} // namespace render
} // namespace eyelib
