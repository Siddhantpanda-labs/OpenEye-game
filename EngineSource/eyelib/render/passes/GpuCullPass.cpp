#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <string>
#include <vector>

namespace eyelib {
namespace render {

namespace {
constexpr uint32_t kMaxIndirectDraws = 256;
constexpr uint32_t kIndirectRegionSize = kMaxIndirectDraws * static_cast<uint32_t>(sizeof(rhi::DrawIndexedIndirectCommand));
constexpr uint32_t kDrawListRegionSize = kMaxIndirectDraws * static_cast<uint32_t>(sizeof(GpuCullDrawEntry));
constexpr const char* kCullCompWithDrawList = "passes/gpu_cull/cull_indirect_with_drawlist";
constexpr const char* kCullCompNoDrawList = "passes/gpu_cull/cull_indirect_no_drawlist";
} // namespace

GpuCullPass::~GpuCullPass() {
    Shutdown();
}

bool GpuCullPass::IsSupported(rhi::IDevice* device) const {
    if (!device) return false;
    const auto& caps = device->GetCapabilities();
    return caps.supportsCompute && caps.supportsMultiDrawIndirect;
}

void GpuCullPass::EnsureResources() {
    if (!m_device || m_initialized || m_initAttempted || !IsSupported(m_device)) return;
    m_initAttempted = true;

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

    std::string compPath;
    if (!m_device->ResolveShaderPath(kCullCompWithDrawList, rhi::ShaderStage::Compute, compPath)) {
        core::Log::ErrorF("GpuCullPass: backend could not resolve path for %s", kCullCompWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
        return;
    }
    if (!platform::filesystem::GetEngineAssetsRoot().empty())
        compPath = platform::filesystem::NormalizePath(platform::filesystem::GetEngineAssetsRoot() + "/" + compPath);
    const bool trySpv = (compPath.find("vulkan") != std::string::npos);
    if (trySpv) {
        std::vector<uint8_t> spvData = platform::filesystem::ReadFile(compPath + ".spv");
        const uint32_t spirvMagic = 0x07230203u;
        if (spvData.size() >= 4 && *reinterpret_cast<uint32_t*>(spvData.data()) == spirvMagic)
            m_computeShaderWithDrawList = m_device->CreateShaderFromBinary(rhi::ShaderStage::Compute, spvData.data(), spvData.size());
    }
    if (!m_computeShaderWithDrawList || !m_computeShaderWithDrawList->IsValid()) {
        std::string compSrc;
        if (!LoadShaderSource(compPath, compSrc)) {
            core::Log::ErrorF("GpuCullPass: failed to load %s", compPath.c_str());
            if (m_computeShaderWithDrawList) m_device->DestroyShader(m_computeShaderWithDrawList);
            m_device->DestroyBuffer(m_indirectBuffer);
            m_indirectBuffer = nullptr;
            return;
        }
        if (m_computeShaderWithDrawList) m_device->DestroyShader(m_computeShaderWithDrawList);
        m_computeShaderWithDrawList = m_device->CreateShader(rhi::ShaderStage::Compute, compSrc.c_str(), compSrc.size());
    }
    if (!m_computeShaderWithDrawList || !m_computeShaderWithDrawList->IsValid()) {
        core::Log::Error("GpuCullPass: failed to create compute shader (with draw list)");
        if (m_computeShaderWithDrawList) m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
        return;
    }
    m_computePipelineWithDrawList = m_device->CreateComputePipeline(m_computeShaderWithDrawList);
    if (!m_computePipelineWithDrawList) {
        core::Log::Error("GpuCullPass: failed to create compute pipeline (with draw list)");
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderWithDrawList = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }

    std::string compNoDrawListPath;
    if (!m_device->ResolveShaderPath(kCullCompNoDrawList, rhi::ShaderStage::Compute, compNoDrawListPath)) {
        core::Log::ErrorF("GpuCullPass: backend could not resolve path for %s", kCullCompNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    if (!platform::filesystem::GetEngineAssetsRoot().empty())
        compNoDrawListPath = platform::filesystem::NormalizePath(platform::filesystem::GetEngineAssetsRoot() + "/" + compNoDrawListPath);
    const bool trySpvNoDrawList = (compNoDrawListPath.find("vulkan") != std::string::npos);
    if (trySpvNoDrawList) {
        std::vector<uint8_t> spvData = platform::filesystem::ReadFile(compNoDrawListPath + ".spv");
        const uint32_t spirvMagic = 0x07230203u;
        if (spvData.size() >= 4 && *reinterpret_cast<uint32_t*>(spvData.data()) == spirvMagic)
            m_computeShaderNoDrawList = m_device->CreateShaderFromBinary(rhi::ShaderStage::Compute, spvData.data(), spvData.size());
    }
    if (!m_computeShaderNoDrawList || !m_computeShaderNoDrawList->IsValid()) {
        std::string compNoDrawListSrc;
        if (!LoadShaderSource(compNoDrawListPath, compNoDrawListSrc)) {
            core::Log::ErrorF("GpuCullPass: failed to load %s", compNoDrawListPath.c_str());
            if (m_computeShaderNoDrawList) m_device->DestroyShader(m_computeShaderNoDrawList);
            m_device->DestroyPipeline(m_computePipelineWithDrawList);
            m_device->DestroyShader(m_computeShaderWithDrawList);
            m_device->DestroyBuffer(m_indirectBuffer);
            m_computePipelineWithDrawList = nullptr;
            m_computeShaderWithDrawList = nullptr;
            m_indirectBuffer = nullptr;
            return;
        }
        if (m_computeShaderNoDrawList) m_device->DestroyShader(m_computeShaderNoDrawList);
        m_computeShaderNoDrawList = m_device->CreateShader(rhi::ShaderStage::Compute, compNoDrawListSrc.c_str(), compNoDrawListSrc.size());
    }
    if (!m_computeShaderNoDrawList || !m_computeShaderNoDrawList->IsValid()) {
        core::Log::Error("GpuCullPass: failed to create compute shader (no draw list)");
        if (m_computeShaderNoDrawList) m_device->DestroyShader(m_computeShaderNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    m_computePipelineNoDrawList = m_device->CreateComputePipeline(m_computeShaderNoDrawList);
    if (!m_computePipelineNoDrawList) {
        core::Log::Error("GpuCullPass: failed to create compute pipeline (no draw list)");
        m_device->DestroyShader(m_computeShaderNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderNoDrawList = nullptr;
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
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
        m_device->DestroyPipeline(m_computePipelineNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderNoDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computePipelineNoDrawList = nullptr;
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderNoDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    m_initialized = true;
}

bool GpuCullPass::Initialize(rhi::IDevice* device) {
    if (!device) return false;
    Shutdown();
    m_device = device;
    EnsureResources();
    return m_initialized;
}

void GpuCullPass::Shutdown() {
    if (!m_device) return;
    if (m_computePipelineNoDrawList) {
        m_device->DestroyPipeline(m_computePipelineNoDrawList);
        m_computePipelineNoDrawList = nullptr;
    }
    if (m_computePipelineWithDrawList) {
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_computePipelineWithDrawList = nullptr;
    }
    if (m_computeShaderNoDrawList) {
        m_device->DestroyShader(m_computeShaderNoDrawList);
        m_computeShaderNoDrawList = nullptr;
    }
    if (m_computeShaderWithDrawList) {
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_computeShaderWithDrawList = nullptr;
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
    m_initAttempted = false;
}

void GpuCullPass::SetDrawList(const GpuCullDrawEntry* entries, uint32_t numDraws) {
    if (entries && numDraws > 0 && m_indirectBuffer) {
        const size_t uploadSize = numDraws * sizeof(GpuCullDrawEntry);
        m_indirectBuffer->Update(entries, kIndirectRegionSize, uploadSize);
        m_drawCount = numDraws;
    } else {
        m_drawCount = 0;  // no draw list: use no-draw-list pipeline (single quad)
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
    if (!cmd || !m_device || !IsSupported(m_device)) return;
    EnsureResources();
    if (!m_computePipelineWithDrawList || !m_computePipelineNoDrawList || !m_indirectBuffer) return;

    const bool useDrawList = (m_drawCount > 0);
    const uint32_t effectiveCount = useDrawList ? (m_drawCount <= kMaxIndirectDraws ? m_drawCount : kMaxIndirectDraws) : 0u;
    m_effectiveDrawCount = (useDrawList && effectiveCount > 0) ? effectiveCount : 1u;

    const uint32_t groupSize = 64u;
    const uint32_t dispatchX = (useDrawList && effectiveCount > 0) ? ((effectiveCount + groupSize - 1u) / groupSize) : 1u;
    const uint32_t dispatchY = 1u;
    const uint32_t dispatchZ = 1u;

    const bool useWithDrawListPipeline = (useDrawList && effectiveCount > 0);
    rhi::IPipeline* pipeline = useWithDrawListPipeline ? m_computePipelineWithDrawList : m_computePipelineNoDrawList;

    cmd->BindPipeline(pipeline);
    if (m_device)
        m_device->EnsureComputePipelineStorageBlockBindings(pipeline);
    cmd->BindStorageBuffer(m_indirectBuffer, 0, 0, kIndirectRegionSize);
    if (m_viewProjUBO && useWithDrawListPipeline)
        cmd->BindUniformBuffer(m_viewProjUBO, 5, 0, static_cast<uint32_t>(16 * sizeof(float)));

    if (useWithDrawListPipeline) {
        const uint32_t drawListBytes = effectiveCount * static_cast<uint32_t>(sizeof(GpuCullDrawEntry));
        cmd->BindStorageBuffer(m_indirectBuffer, 1, kIndirectRegionSize, drawListBytes);
        cmd->SetUniformUint("u_NumDraws", effectiveCount);
        const bool useOcclusion = (m_occlusionDepthTex != nullptr && m_occlusionDepthWidth > 0 && m_occlusionDepthHeight > 0);
        cmd->SetUniformInt("u_UseOcclusionCull", useOcclusion ? 1 : 0);
        if (useOcclusion) {
            cmd->BindTexture(m_occlusionDepthTex, 4);
            float depthSize[2] = { static_cast<float>(m_occlusionDepthWidth), static_cast<float>(m_occlusionDepthHeight) };
            cmd->SetUniformVec2("u_DepthSize", depthSize);
        }
    } else {
        cmd->SetUniformUint("u_NumDraws", 0);
    }
    cmd->Dispatch(dispatchX, dispatchY, dispatchZ);

    rhi::BarrierBufferForIndirectDraw(cmd, m_indirectBuffer);
}

} // namespace render
} // namespace eyelib
