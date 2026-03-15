#include "eyelib/render/lighting/LightCullPass.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <cstring>
#include <vector>

namespace eyelib {
namespace render {

namespace {
constexpr const char* kLightCullLogicalName = "passes/light_cull/light_cull";
constexpr const char* kLightCullCompPathFallback = "assets/shaders/opengl/passes/light_cull/light_cull.comp";
} // namespace

LightCullPass::~LightCullPass() {
    Shutdown();
}

bool LightCullPass::IsSupported(rhi::IDevice* device) const {
    return device != nullptr && device->GetCapabilities().supportsCompute;
}

void LightCullPass::EnsureResources() {
    if (!m_device || m_initialized || m_initAttempted || !IsSupported(m_device)) return;
    m_initAttempted = true;

    std::string path;
    if (m_device->ResolveShaderPath(kLightCullLogicalName, rhi::ShaderStage::Compute, path))
        path = platform::filesystem::GetEngineAssetsRoot().empty() ? path : platform::filesystem::NormalizePath(platform::filesystem::GetEngineAssetsRoot() + "/" + path);
    if (path.empty()) {
        path = platform::filesystem::GetEngineAssetsRoot().empty() ? kLightCullCompPathFallback : platform::filesystem::NormalizePath(platform::filesystem::GetEngineAssetsRoot() + "/" + kLightCullCompPathFallback);
    }

    std::string compSrc;
    const bool trySpv = (path.find("vulkan") != std::string::npos);
    if (trySpv) {
        std::string spvPath = path + ".spv";
        std::vector<uint8_t> spvData = platform::filesystem::ReadFile(spvPath);
        const uint32_t spirvMagic = 0x07230203u;
        if (spvData.size() >= 4 && *reinterpret_cast<uint32_t*>(spvData.data()) == spirvMagic) {
            m_computeShader = m_device->CreateShaderFromBinary(rhi::ShaderStage::Compute, spvData.data(), spvData.size());
            if (m_computeShader && m_computeShader->IsValid())
                goto create_pipeline;
            if (m_computeShader) { m_device->DestroyShader(m_computeShader); m_computeShader = nullptr; }
        }
    }
    if (!LoadShaderSource(path, compSrc)) {
        core::Log::ErrorF("LightCullPass: failed to load %s", path.c_str());
        return;
    }
    m_computeShader = m_device->CreateShader(rhi::ShaderStage::Compute, compSrc.c_str(), compSrc.size());
    if (!m_computeShader || !m_computeShader->IsValid()) {
        core::Log::Error("LightCullPass: failed to create compute shader");
        if (m_computeShader) m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
        return;
    }
create_pipeline:
    m_computePipeline = m_device->CreateComputePipelineWithLayout(m_computeShader, rhi::PipelineLayoutKind::ComputeLightCull);
    if (!m_computePipeline) {
        m_computePipeline = m_device->CreateComputePipeline(m_computeShader);
    }
    if (!m_computePipeline) {
        core::Log::Error("LightCullPass: failed to create compute pipeline");
        m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
        return;
    }
    rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
    rhi::IPipelineLayout* lightCullLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::LightCullPass);
    rhi::IPipelineLayout* sceneLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::SceneBuffers);
    if (frameLayout && lightCullLayout && sceneLayout) {
        m_frameSet = m_device->CreateDescriptorSet(frameLayout);
        m_lightCullPassSet = m_device->CreateDescriptorSet(lightCullLayout);
        m_sceneBuffersSet = m_device->CreateDescriptorSet(sceneLayout);
        rhi::BufferDesc paramsDesc;
        paramsDesc.sizeInBytes = 256;
        paramsDesc.usage = rhi::BufferUsage::Uniform;
        paramsDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_paramsUBO = m_device->CreateBuffer(paramsDesc, nullptr);
        if (m_paramsUBO && m_frameSet) {
            m_frameSet->UpdateUniformBuffer(0, m_paramsUBO, 0, 128);
            m_frameSet->UpdateUniformBuffer(1, m_paramsUBO, 128, 128);
        }
    }
    m_initialized = true;
}

bool LightCullPass::Initialize(rhi::IDevice* device) {
    if (!device) return false;
    Shutdown();
    m_device = device;
    EnsureResources();
    return m_initialized;
}

void LightCullPass::Shutdown() {
    if (!m_device) return;
    if (m_frameSet) { m_device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
    if (m_lightCullPassSet) { m_device->DestroyDescriptorSet(m_lightCullPassSet); m_lightCullPassSet = nullptr; }
    if (m_sceneBuffersSet) { m_device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
    if (m_paramsUBO) { m_device->DestroyBuffer(m_paramsUBO); m_paramsUBO = nullptr; }
    if (m_computePipeline) {
        m_device->DestroyPipeline(m_computePipeline);
        m_computePipeline = nullptr;
    }
    if (m_computeShader) {
        m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
    }
    m_pointLightBuffer = nullptr;
    m_spotLightBuffer = nullptr;
    m_outputBuffer = nullptr;
    m_initialized = false;
    m_initAttempted = false;
}

void LightCullPass::SetViewProj(const float* view16, const float* proj16) {
    if (view16) std::memcpy(m_view, view16, 16 * sizeof(float));
    if (proj16) std::memcpy(m_proj, proj16, 16 * sizeof(float));
}

void LightCullPass::SetParams(uint32_t tileCountX, uint32_t tileCountY, uint32_t sliceCount,
                              float logNear, float logFar, float nearPlane, float farPlane,
                              uint32_t screenWidth, uint32_t screenHeight) {
    m_tileCountX = tileCountX;
    m_tileCountY = tileCountY;
    m_sliceCount = sliceCount;
    m_logNear = logNear;
    m_logFar = logFar;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
}

void LightCullPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_device || !IsSupported(m_device)) return;
    EnsureResources();
    if (!m_computePipeline || !m_outputBuffer) return;

    const uint32_t totalClusters = m_tileCountX * m_tileCountY * m_sliceCount;
    if (totalClusters == 0) return;

    cmd->BindPipeline(m_computePipeline);

    if (m_frameSet && m_lightCullPassSet && m_sceneBuffersSet && m_paramsUBO) {
        struct ParamsBlock {
            float view[16];
            float proj[16];
            uint32_t tileCountX, tileCountY, sliceCount;
            float logNear, logFar, nearPlane, farPlane, screenWidth, screenHeight;
            int32_t pointLightCount, spotLightCount;
        };
        ParamsBlock params = {};
        std::memcpy(params.view, m_view, sizeof(params.view));
        std::memcpy(params.proj, m_proj, sizeof(params.proj));
        params.tileCountX = m_tileCountX;
        params.tileCountY = m_tileCountY;
        params.sliceCount = m_sliceCount;
        params.logNear = m_logNear;
        params.logFar = m_logFar;
        params.nearPlane = m_nearPlane;
        params.farPlane = m_farPlane;
        params.screenWidth = static_cast<float>(m_screenWidth);
        params.screenHeight = static_cast<float>(m_screenHeight);
        params.pointLightCount = static_cast<int32_t>(m_pointLightCount);
        params.spotLightCount = static_cast<int32_t>(m_spotLightCount);
        m_paramsUBO->Update(&params, 0, sizeof(params));
        m_lightCullPassSet->UpdateUniformBuffer(0, m_paramsUBO, 0, sizeof(params));
        m_lightCullPassSet->UpdateStorageBuffer(1, m_outputBuffer, 0, 0);
        if (m_pointLightBuffer) m_sceneBuffersSet->UpdateStorageBuffer(2, m_pointLightBuffer, 0, 0);
        if (m_spotLightBuffer) m_sceneBuffersSet->UpdateStorageBuffer(3, m_spotLightBuffer, 0, 0);
        rhi::IDescriptorSet* emptySet = m_device->GetEmptyResourceSet();
        if (emptySet) {
            rhi::IDescriptorSet* sets[5] = { m_frameSet, m_lightCullPassSet, emptySet, emptySet, m_sceneBuffersSet };
            cmd->BindDescriptorSets(0, 5, sets);
        } else {
            rhi::IDescriptorSet* sets[3] = { m_frameSet, m_lightCullPassSet, m_sceneBuffersSet };
            cmd->BindDescriptorSets(0, 3, sets);
        }
    } else {
        cmd->BindStorageBuffer(m_outputBuffer, 0, 0, 0);
        if (m_pointLightBuffer) cmd->BindStorageBuffer(m_pointLightBuffer, 2, 0, 0);
        if (m_spotLightBuffer) cmd->BindStorageBuffer(m_spotLightBuffer, 3, 0, 0);
        cmd->SetUniformMat4("u_View", m_view);
        cmd->SetUniformMat4("u_Projection", m_proj);
        cmd->SetUniformUint("u_TileCountX", m_tileCountX);
        cmd->SetUniformUint("u_TileCountY", m_tileCountY);
        cmd->SetUniformUint("u_SliceCount", m_sliceCount);
        cmd->SetUniformFloat("u_LogNear", m_logNear);
        cmd->SetUniformFloat("u_LogFar", m_logFar);
        cmd->SetUniformFloat("u_NearPlane", m_nearPlane);
        cmd->SetUniformFloat("u_FarPlane", m_farPlane);
        cmd->SetUniformFloat("u_ScreenWidth", static_cast<float>(m_screenWidth));
        cmd->SetUniformFloat("u_ScreenHeight", static_cast<float>(m_screenHeight));
        cmd->SetUniformInt("u_PointLightCount", static_cast<int>(m_pointLightCount));
        cmd->SetUniformInt("u_SpotLightCount", static_cast<int>(m_spotLightCount));
    }

    const uint32_t groupSize = 64u;
    const uint32_t groups = (totalClusters + groupSize - 1u) / groupSize;
    cmd->Dispatch(groups, 1, 1);
}

} // namespace render
} // namespace eyelib
