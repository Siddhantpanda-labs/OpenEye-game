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
constexpr const char* kCullCompWithDrawListRs = "passes/gpu_cull/cull_indirect_with_drawlist_rs";
constexpr const char* kCullCompNoDrawListRs = "passes/gpu_cull/cull_indirect_no_drawlist_rs";
} // namespace

GpuCullPass::~GpuCullPass() {
    Shutdown();
}

bool GpuCullPass::IsSupported(rhi::IDevice* device) const {
    if (!device) return false;
    const auto& caps = device->GetCapabilities();
    return caps.supportsCompute && caps.supportsMultiDrawIndirect;
}

static rhi::IShader* LoadComputeShader(rhi::IDevice* device, const char* logicalName, std::string& outPath) {
    if (!device->ResolveShaderPath(logicalName, rhi::ShaderStage::Compute, outPath)) return nullptr;
    if (!platform::filesystem::GetEngineAssetsRoot().empty())
        outPath = platform::filesystem::NormalizePath(platform::filesystem::GetEngineAssetsRoot() + "/" + outPath);
    rhi::IShader* sh = nullptr;
    if (outPath.find("vulkan") != std::string::npos) {
        std::vector<uint8_t> spvData = platform::filesystem::ReadFile(outPath + ".spv");
        const uint32_t spirvMagic = 0x07230203u;
        if (spvData.size() >= 4 && *reinterpret_cast<uint32_t*>(spvData.data()) == spirvMagic)
            sh = device->CreateShaderFromBinary(rhi::ShaderStage::Compute, spvData.data(), spvData.size());
    }
    if (!sh || !sh->IsValid()) {
        std::string src;
        if (!LoadShaderSource(outPath, src)) return sh;
        if (sh) device->DestroyShader(sh);
        sh = device->CreateShader(rhi::ShaderStage::Compute, src.c_str(), src.size());
    }
    return sh;
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

    rhi::BufferDesc uboDesc;
    uboDesc.sizeInBytes = 16 * sizeof(float);
    uboDesc.usage = rhi::BufferUsage::Uniform;
    uboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_viewProjUBO = m_device->CreateBuffer(uboDesc, nullptr);
    if (!m_viewProjUBO) {
        core::Log::Error("GpuCullPass: failed to create view-proj UBO");
        m_device->DestroyBuffer(m_indirectBuffer);
        m_indirectBuffer = nullptr;
        return;
    }

    // Prefer resource-space layout (ComputeGpuCull: set 0 Frame, set 4 SceneBuffers)
    rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
    rhi::IPipelineLayout* sceneLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::SceneBuffers);
    if (frameLayout && sceneLayout) {
        std::string pathRs;
        rhi::IShader* shWithRs = LoadComputeShader(m_device, kCullCompWithDrawListRs, pathRs);
        if (shWithRs && shWithRs->IsValid()) {
            std::string pathNoRs;
            rhi::IShader* shNoRs = LoadComputeShader(m_device, kCullCompNoDrawListRs, pathNoRs);
            if (shNoRs && shNoRs->IsValid()) {
                m_computePipelineWithDrawList = m_device->CreateComputePipelineWithLayout(shWithRs, rhi::PipelineLayoutKind::ComputeGpuCull);
                m_computePipelineNoDrawList = m_device->CreateComputePipelineWithLayout(shNoRs, rhi::PipelineLayoutKind::ComputeGpuCull);
                if (m_computePipelineWithDrawList && m_computePipelineNoDrawList) {
                    m_computeShaderWithDrawList = shWithRs;
                    m_computeShaderNoDrawList = shNoRs;
                    m_frameSet = m_device->CreateDescriptorSet(frameLayout);
                    m_sceneBuffersSet = m_device->CreateDescriptorSet(sceneLayout);
                    if (m_frameSet && m_sceneBuffersSet) {
                        m_frameSet->UpdateUniformBuffer(0, m_viewProjUBO, 0, 16 * sizeof(float));
                        m_sceneBuffersSet->UpdateStorageBuffer(0, m_indirectBuffer, kIndirectRegionSize, kDrawListRegionSize);
                        m_sceneBuffersSet->UpdateStorageBuffer(1, m_indirectBuffer, 0, kIndirectRegionSize);
                        m_useResourceSpace = true;
                        m_initialized = true;
                        return;
                    }
                    if (m_frameSet) { m_device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
                    if (m_sceneBuffersSet) { m_device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
                }
                if (m_computePipelineWithDrawList) { m_device->DestroyPipeline(m_computePipelineWithDrawList); m_computePipelineWithDrawList = nullptr; }
                if (m_computePipelineNoDrawList) { m_device->DestroyPipeline(m_computePipelineNoDrawList); m_computePipelineNoDrawList = nullptr; }
            }
            if (shNoRs) m_device->DestroyShader(shNoRs);
            if (shWithRs) m_device->DestroyShader(shWithRs);
        } else if (shWithRs) {
            m_device->DestroyShader(shWithRs);
        }
    }

    // Legacy path
    std::string compPath;
    m_computeShaderWithDrawList = LoadComputeShader(m_device, kCullCompWithDrawList, compPath);
    if (!m_computeShaderWithDrawList || !m_computeShaderWithDrawList->IsValid()) {
        core::Log::ErrorF("GpuCullPass: failed to load %s", kCullCompWithDrawList);
        if (m_computeShaderWithDrawList) m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_viewProjUBO);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderWithDrawList = nullptr;
        m_viewProjUBO = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    m_computePipelineWithDrawList = m_device->CreateComputePipeline(m_computeShaderWithDrawList);
    if (!m_computePipelineWithDrawList) {
        core::Log::Error("GpuCullPass: failed to create compute pipeline (with draw list)");
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_viewProjUBO);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderWithDrawList = nullptr;
        m_viewProjUBO = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }

    std::string compNoDrawListPath;
    m_computeShaderNoDrawList = LoadComputeShader(m_device, kCullCompNoDrawList, compNoDrawListPath);
    if (!m_computeShaderNoDrawList || !m_computeShaderNoDrawList->IsValid()) {
        core::Log::ErrorF("GpuCullPass: failed to load %s", kCullCompNoDrawList);
        if (m_computeShaderNoDrawList) m_device->DestroyShader(m_computeShaderNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_viewProjUBO);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderNoDrawList = nullptr;
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
        m_viewProjUBO = nullptr;
        m_indirectBuffer = nullptr;
        return;
    }
    m_computePipelineNoDrawList = m_device->CreateComputePipeline(m_computeShaderNoDrawList);
    if (!m_computePipelineNoDrawList) {
        core::Log::Error("GpuCullPass: failed to create compute pipeline (no draw list)");
        m_device->DestroyShader(m_computeShaderNoDrawList);
        m_device->DestroyPipeline(m_computePipelineWithDrawList);
        m_device->DestroyShader(m_computeShaderWithDrawList);
        m_device->DestroyBuffer(m_viewProjUBO);
        m_device->DestroyBuffer(m_indirectBuffer);
        m_computeShaderNoDrawList = nullptr;
        m_computePipelineWithDrawList = nullptr;
        m_computeShaderWithDrawList = nullptr;
        m_viewProjUBO = nullptr;
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
    if (m_frameSet) { m_device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
    if (m_sceneBuffersSet) { m_device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
    m_useResourceSpace = false;
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

    if (m_useResourceSpace && m_frameSet && m_sceneBuffersSet) {
        rhi::IDescriptorSet* emptySet = m_device->GetEmptyResourceSet();
        if (emptySet) {
            rhi::IDescriptorSet* sets[5] = { m_frameSet, emptySet, emptySet, emptySet, m_sceneBuffersSet };
            cmd->BindDescriptorSets(0, 5, sets);
        } else {
            rhi::IDescriptorSet* sets[2] = { m_frameSet, m_sceneBuffersSet };
            cmd->BindDescriptorSets(0, 2, sets);
        }
    } else {
        m_device->EnsureComputePipelineStorageBlockBindings(pipeline);
        cmd->BindStorageBuffer(m_indirectBuffer, 0, 0, kIndirectRegionSize);
        if (m_viewProjUBO && useWithDrawListPipeline)
            cmd->BindUniformBuffer(m_viewProjUBO, 5, 0, static_cast<uint32_t>(16 * sizeof(float)));
    }

    if (useWithDrawListPipeline) {
        cmd->SetUniformUint("u_NumDraws", effectiveCount);
        const bool useOcclusion = !m_useResourceSpace && (m_occlusionDepthTex != nullptr && m_occlusionDepthWidth > 0 && m_occlusionDepthHeight > 0);
        cmd->SetUniformInt("u_UseOcclusionCull", useOcclusion ? 1 : 0);
        if (useOcclusion) {
            cmd->BindTexture(m_occlusionDepthTex, 4);
            float depthSize[2] = { static_cast<float>(m_occlusionDepthWidth), static_cast<float>(m_occlusionDepthHeight) };
            cmd->SetUniformVec2("u_DepthSize", depthSize);
        }
    } else {
        cmd->SetUniformUint("u_NumDraws", 0);
    }
    if (!m_useResourceSpace && useWithDrawListPipeline) {
        const uint32_t drawListBytes = effectiveCount * static_cast<uint32_t>(sizeof(GpuCullDrawEntry));
        cmd->BindStorageBuffer(m_indirectBuffer, 1, kIndirectRegionSize, drawListBytes);
    }
    cmd->Dispatch(dispatchX, dispatchY, dispatchZ);

    rhi::BarrierBufferForIndirectDraw(cmd, m_indirectBuffer);
}

} // namespace render
} // namespace eyelib
