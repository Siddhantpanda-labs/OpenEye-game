#include "eyelib/fluids/FluidVolumeRenderer.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace fluids {

FluidVolumeRenderer::FluidVolumeRenderer() = default;
FluidVolumeRenderer::~FluidVolumeRenderer() { Shutdown(); }

bool FluidVolumeRenderer::Initialize() {
    m_device = render::rhi::GetDefaultDevice();
    if (!m_device) {
        core::Log::Error("FluidVolumeRenderer::Initialize: no RHI device (set default device first)");
        return false;
    }
    CreateCube();
    EnsureShaders();
    if (!m_backgroundTexture) {
        m_backgroundTexture = std::make_shared<render::Texture2D>();
        const uint8_t black[4] = { 0, 0, 0, 255 };
        m_backgroundTexture->Create(m_device, 1, 1, render::TextureFormat::RGBA8, black);
    }
    core::Log::Info("FluidVolumeRenderer initialized");
    return true;
}

void FluidVolumeRenderer::Shutdown() {
    if (m_device) {
        if (m_cubeVB) { m_device->DestroyBuffer(m_cubeVB); m_cubeVB = nullptr; }
        if (m_rayDataFrontRT) { m_device->DestroyRenderTarget(m_rayDataFrontRT); m_rayDataFrontRT = nullptr; }
        if (m_rayDataBackRT) { m_device->DestroyRenderTarget(m_rayDataBackRT); m_rayDataBackRT = nullptr; }
    }
    m_device = nullptr;
    m_densityTexture.reset();
    m_obstacleTexture.reset();
    m_backgroundTexture.reset();
}

void FluidVolumeRenderer::SetViewport(unsigned int width, unsigned int height) {
    if (width == m_viewportW && height == m_viewportH) return;
    m_viewportW = width;
    m_viewportH = height;
    if (m_device) {
        if (m_rayDataFrontRT) { m_device->DestroyRenderTarget(m_rayDataFrontRT); m_rayDataFrontRT = nullptr; }
        if (m_rayDataBackRT) { m_device->DestroyRenderTarget(m_rayDataBackRT); m_rayDataBackRT = nullptr; }
    }
}

void FluidVolumeRenderer::UpdateFromSimulation(const FluidSimulation& sim, float densityScale) {
    if (!sim.IsInitialized()) return;
    m_device = render::rhi::GetDefaultDevice();
    if (!m_device) return;
    const auto desc = sim.GetDesc();
    const math::IVec3 res = desc.resolution;
    if (res.x <= 0 || res.y <= 0 || res.z <= 0) return;

    if (!m_densityTexture || !m_obstacleTexture ||
        m_cachedResolution.x != res.x || m_cachedResolution.y != res.y || m_cachedResolution.z != res.z) {
        m_densityTexture = std::make_shared<render::Texture3D>();
        m_obstacleTexture = std::make_shared<render::Texture3D>();
        m_densityTexture->Create(m_device, res.x, res.y, res.z, render::TextureFormat::R8);
        m_obstacleTexture->Create(m_device, res.x, res.y, res.z, render::TextureFormat::R8);
        m_cachedResolution = res;
    }

    const auto& density = sim.GetDensityField();
    const auto& obstacles = sim.GetObstacleMask();
    const size_t count = density.size();
    if (count == 0 || obstacles.size() != count) return;

    std::vector<uint8_t> densityBytes(count);
    std::vector<uint8_t> obstacleBytes(count);
    const float scale = std::max(0.0001f, densityScale);
    for (size_t i = 0; i < count; ++i) {
        const float d = std::max(0.0f, std::min(1.0f, density[i] * scale));
        densityBytes[i] = static_cast<uint8_t>(d * 255.0f);
        obstacleBytes[i] = obstacles[i] ? 255 : 0;
    }

    m_densityTexture->SetData(densityBytes.data());
    m_obstacleTexture->SetData(obstacleBytes.data());
}

void FluidVolumeRenderer::SetBackgroundTexture(const std::shared_ptr<render::Texture2D>& background) {
    if (background && background->IsValid()) {
        m_backgroundTexture = background;
    }
}

void FluidVolumeRenderer::RenderVolume(const FluidSimulation& sim,
                                       const render::CameraView& camera,
                                       const math::Mat4& model,
                                       const math::Vec3& emissionPos01,
                                       const math::Vec3& volumeSize,
                                       render::rhi::IRenderTarget* target) {
    if (!sim.IsInitialized() || !m_device || !target) return;
    m_device = render::rhi::GetDefaultDevice();
    if (!m_device) return;

    EnsureRayDataTargets();
    EnsureShaders();
    if (!m_densityTexture || !m_obstacleTexture) {
        UpdateFromSimulation(sim, 1.0f);
        if (!m_densityTexture || !m_obstacleTexture) return;
    }

    const auto desc = sim.GetDesc();
    const math::IVec3 res = desc.resolution;
    if (!m_rayDataBackRT || !m_rayDataFrontRT || !m_cubeVB) return;

    render::rhi::ICommandList* cmd = m_device->GetImmediateCommandList();
    if (!cmd) return;

    const unsigned int w = m_viewportW;
    const unsigned int h = m_viewportH;
    render::rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };

    cmd->SetBlend(false);
    cmd->SetCullFace(true);
    cmd->SetDepthTest(true);

    // Back faces (exit points)
    cmd->SetCullFrontFace(true);
    cmd->SetRenderTarget(m_rayDataBackRT);
    cmd->SetViewport(0, 0, w, h);
    cmd->ClearRenderTarget(m_rayDataBackRT, clearBlack);
    m_shRayDataBack->Bind();
    m_shRayDataBack->SetMat4("model", model);
    m_shRayDataBack->SetMat4("view", camera.view);
    m_shRayDataBack->SetMat4("projection", camera.projection);
    cmd->BindVertexBuffer(m_cubeVB, 0);
    cmd->Draw(36, 0);

    // Front faces (entry points)
    cmd->SetCullFrontFace(false);
    cmd->SetRenderTarget(m_rayDataFrontRT);
    cmd->SetViewport(0, 0, w, h);
    cmd->ClearRenderTarget(m_rayDataFrontRT, clearBlack);
    m_shRayDataFront->Bind();
    m_shRayDataFront->SetMat4("model", model);
    m_shRayDataFront->SetMat4("view", camera.view);
    m_shRayDataFront->SetMat4("projection", camera.projection);
    cmd->BindVertexBuffer(m_cubeVB, 0);
    cmd->Draw(36, 0);

    // Raymarch to target
    cmd->SetRenderTarget(target);
    cmd->SetViewport(0, 0, target->GetWidth(), target->GetHeight());
    m_shRaymarch->Bind();
    m_shRaymarch->SetMat4("model", model);
    m_shRaymarch->SetMat4("view", camera.view);
    m_shRaymarch->SetMat4("projection", camera.projection);
    m_shRaymarch->SetVec3("grid_size", math::Vec3(static_cast<float>(res.x), static_cast<float>(res.y), static_cast<float>(res.z)));
    m_shRaymarch->SetVec3("u_EmissionPos", emissionPos01);
    m_shRaymarch->SetVec3("u_VolumeSize", volumeSize);
    m_shRaymarch->SetInt("u_FluidType", sim.GetType() == FluidType::Liquid ? 1 : 0);

    const math::Mat4 invView = glm::inverse(camera.view);
    const math::Vec3 right = math::Vec3(invView[0]);
    const math::Vec3 up = math::Vec3(invView[1]);
    const math::Vec3 forward = math::Vec3(-invView[2]);

    m_shRaymarch->SetVec3("eyePos", camera.viewPos);
    m_shRaymarch->SetVec3("cameraRight", right);
    m_shRaymarch->SetVec3("cameraUp", up);
    m_shRaymarch->SetVec3("cameraFront", forward);
    m_shRaymarch->SetFloat("nearPlane", camera.nearPlane);
    m_shRaymarch->SetVec2("InverseScreenSize", math::Vec2(1.0f / std::max(1u, m_viewportW),
                                                          1.0f / std::max(1u, m_viewportH)));

    render::rhi::ITexture* rayBackTex = m_rayDataBackRT->GetColorTexture(0);
    render::rhi::ITexture* rayFrontTex = m_rayDataFrontRT->GetColorTexture(0);
    if (rayBackTex) cmd->BindTexture(rayBackTex, 0);
    if (rayFrontTex) cmd->BindTexture(rayFrontTex, 1);
    m_shRaymarch->SetInt("RayDataBack", 0);
    m_shRaymarch->SetInt("RayDataFront", 1);

    m_densityTexture->Bind(2);
    m_shRaymarch->SetInt("DensityTexture", 2);
    m_obstacleTexture->Bind(3);
    m_shRaymarch->SetInt("ObstacleTexture", 3);
    if (m_backgroundTexture) {
        m_backgroundTexture->Bind(4);
        m_shRaymarch->SetInt("BackgroundTexture", 4);
    }

    cmd->BindVertexBuffer(m_cubeVB, 0);
    cmd->Draw(36, 0);

    cmd->SetRenderTarget(nullptr);
}

void FluidVolumeRenderer::EnsureRayDataTargets() {
    if (!m_device || m_viewportW == 0 || m_viewportH == 0) return;
    if (m_rayDataFrontRT && m_rayDataBackRT) return;

    const unsigned int w = m_viewportW;
    const unsigned int h = m_viewportH;
    std::vector<render::rhi::RHITextureFormat> colorFormats = { render::rhi::RHITextureFormat::RGBA16F };
    m_rayDataBackRT = m_device->CreateRenderTarget(w, h, colorFormats, render::rhi::RHITextureFormat::None, 1);
    m_rayDataFrontRT = m_device->CreateRenderTarget(w, h, colorFormats, render::rhi::RHITextureFormat::None, 1);
}

void FluidVolumeRenderer::EnsureShaders() {
    if (!m_device) m_device = render::rhi::GetDefaultDevice();
    if (!m_device) return;

    render::rhi::VertexLayout posOnly;
    posOnly.strideInBytes = 12;
    posOnly.attributes = { { 0, render::rhi::VertexAttributeType::Float3, 0 } };

    if (!m_shRayDataBack) {
        m_shRayDataBack = std::make_shared<render::Shader>();
        m_shRayDataBack->LoadFromFile(m_device, "assets/shaders/fluids/rendering/raydata.vert",
                                     "assets/shaders/fluids/rendering/raydata_back.frag", posOnly);
    }
    if (!m_shRayDataFront) {
        m_shRayDataFront = std::make_shared<render::Shader>();
        m_shRayDataFront->LoadFromFile(m_device, "assets/shaders/fluids/rendering/raydata.vert",
                                      "assets/shaders/fluids/rendering/raydata_front.frag", posOnly);
    }
    if (!m_shRaymarch) {
        m_shRaymarch = std::make_shared<render::Shader>();
        m_shRaymarch->LoadFromFile(m_device, "assets/shaders/fluids/rendering/raydata.vert",
                                  "assets/shaders/fluids/rendering/raymarching.frag", posOnly);
    }
}

void FluidVolumeRenderer::CreateCube() {
    static const float verts[] = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
         1.0f,-1.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f, -1.0f, 1.0f,-1.0f, -1.0f, 1.0f, 1.0f
    };
    if (!m_device) m_device = render::rhi::GetDefaultDevice();
    if (!m_device) return;

    render::rhi::BufferDesc bd;
    bd.sizeInBytes = sizeof(verts);
    bd.usage = render::rhi::BufferUsage::Vertex;
    bd.memoryHint = render::rhi::BufferMemoryHint::DeviceOnly;
    m_cubeVB = m_device->CreateBuffer(bd, verts);
}

} // namespace fluids
} // namespace eyelib
