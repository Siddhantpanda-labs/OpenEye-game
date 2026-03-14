#include "eyelib/render/lighting/LightCullPass.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <cstring>

namespace eyelib {
namespace render {

namespace {
constexpr const char* kLightCullCompPath = "assets/shaders/opengl/passes/light_cull/light_cull.comp";
} // namespace

LightCullPass::~LightCullPass() {
    Shutdown();
}

bool LightCullPass::IsSupported(rhi::IDevice* device) const {
    return device != nullptr && device->GetCapabilities().supportsCompute;
}

void LightCullPass::EnsureResources() {
    if (!m_device || m_initialized || !IsSupported(m_device)) return;

    std::string path = platform::filesystem::GetEngineAssetsRoot();
    if (!path.empty())
        path = platform::filesystem::NormalizePath(path + "/" + kLightCullCompPath);
    else
        path = kLightCullCompPath;

    std::string compSrc;
    if (!LoadShaderSource(path, compSrc)) {
        core::Log::ErrorF("LightCullPass: failed to load %s", kLightCullCompPath);
        return;
    }
    m_computeShader = m_device->CreateShader(rhi::ShaderStage::Compute, compSrc.c_str(), compSrc.size());
    if (!m_computeShader || !m_computeShader->IsValid()) {
        core::Log::Error("LightCullPass: failed to create compute shader");
        if (m_computeShader) m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
        return;
    }
    m_computePipeline = m_device->CreateComputePipeline(m_computeShader);
    if (!m_computePipeline) {
        core::Log::Error("LightCullPass: failed to create compute pipeline");
        m_device->DestroyShader(m_computeShader);
        m_computeShader = nullptr;
        return;
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
    cmd->BindStorageBuffer(m_outputBuffer, 0, 0, 0);
    if (m_pointLightBuffer)
        cmd->BindStorageBuffer(m_pointLightBuffer, 3, 0, 0);
    if (m_spotLightBuffer)
        cmd->BindStorageBuffer(m_spotLightBuffer, 4, 0, 0);

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

    const uint32_t groupSize = 64u;
    const uint32_t groups = (totalClusters + groupSize - 1u) / groupSize;
    cmd->Dispatch(groups, 1, 1);
}

} // namespace render
} // namespace eyelib
