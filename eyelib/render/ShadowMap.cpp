#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eyelib {
namespace render {

ShadowMap::ShadowMap() = default;

ShadowMap::~ShadowMap() { Destroy(); }

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_resolution(other.m_resolution),
      m_deviceRHI(other.m_deviceRHI),
      m_renderTargetRHI(other.m_renderTargetRHI),
      m_lightView(other.m_lightView),
      m_lightProj(other.m_lightProj),
      m_shadowMatrix(other.m_shadowMatrix) {
    other.m_deviceRHI = nullptr;
    other.m_renderTargetRHI = nullptr;
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_resolution = other.m_resolution;
        m_deviceRHI = other.m_deviceRHI;
        m_renderTargetRHI = other.m_renderTargetRHI;
        m_lightView = other.m_lightView;
        m_lightProj = other.m_lightProj;
        m_shadowMatrix = other.m_shadowMatrix;
        other.m_deviceRHI = nullptr;
        other.m_renderTargetRHI = nullptr;
    }
    return *this;
}

bool ShadowMap::Initialize(uint32_t resolution) {
    return Initialize(nullptr, resolution);
}

bool ShadowMap::Initialize(rhi::IDevice* device, uint32_t resolution) {
    if (!resolution || !device) return false;
    Destroy();
    m_resolution = resolution;
    m_deviceRHI = device;

    std::vector<rhi::RHITextureFormat> noColor;
    m_renderTargetRHI = device->CreateRenderTarget(
        m_resolution, m_resolution, noColor, rhi::RHITextureFormat::Depth24, 1);
    if (!m_renderTargetRHI) {
        m_deviceRHI = nullptr;
        return false;
    }
    return true;
}

void ShadowMap::UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                               const math::Vec3& cameraPos, float nearPlane, float farPlane) {
    (void)view;
    (void)proj;
    glm::vec3 dir(lightDir.x, lightDir.y, lightDir.z);
    float len = glm::length(dir);
    if (len < 1e-6f) len = 1.f;
    dir /= len;
    glm::vec3 center(cameraPos.x, cameraPos.y, cameraPos.z);
    const float lightDistance = 80.f;
    glm::vec3 lightEye = center - dir * lightDistance;
    glm::vec3 up(0.f, 1.f, 0.f);
    if (std::abs(glm::dot(up, dir)) > 0.99f) up = glm::vec3(0.f, 0.f, 1.f);
    glm::mat4 lightView = glm::lookAt(lightEye, center, up);
    float extent = 55.f;
    const float orthoNear = 1.f, orthoFar = 200.f;
    glm::mat4 lightProj = glm::ortho(-extent, extent, -extent, extent, orthoNear, orthoFar);
    m_lightView = math::Mat4(lightView);
    m_lightProj = math::Mat4(lightProj);
    m_shadowMatrix = m_lightProj * m_lightView;
}

void ShadowMap::BindForShadowPass(rhi::ICommandList* cmd) {
    if (!cmd || !m_renderTargetRHI) return;
    rhi::ClearValue cv = {};
    cv.depth = 1.f;
    rhi::ClearAndSetRenderTargetWithViewport(cmd, m_renderTargetRHI, 0, 0, m_resolution, m_resolution, cv);
}

void ShadowMap::Unbind(rhi::ICommandList* cmd) {
    if (cmd) cmd->SetRenderTarget(nullptr);
}

unsigned int ShadowMap::GetDepthTexture() const {
    rhi::ITexture* depthTex = GetDepthTextureRHI();
    if (!depthTex) return 0;
    void* native = depthTex->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0;
}

void ShadowMap::Destroy() {
    if (m_deviceRHI && m_renderTargetRHI) {
        m_deviceRHI->DestroyRenderTarget(m_renderTargetRHI);
        m_renderTargetRHI = nullptr;
    }
    m_deviceRHI = nullptr;
}

} // namespace render
} // namespace eyelib
