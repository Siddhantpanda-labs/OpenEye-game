#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace eyelib {
namespace render {

namespace {
math::Vec3 getCameraFront(const math::Mat4& view) {
    return math::Vec3(-view[0][2], -view[1][2], -view[2][2]);
}
math::Vec3 getCameraRight(const math::Mat4& view) {
    return math::Vec3(view[0][0], view[1][0], view[2][0]);
}
math::Vec3 getCameraUp(const math::Mat4& view) {
    return math::Vec3(view[0][1], view[1][1], view[2][1]);
}
void deriveFovAspectFromProjection(const math::Mat4& proj, float& outFov, float& outAspect) {
    const float tanHalfFov = 1.0f / proj[1][1];
    outFov = 2.0f * std::atan(tanHalfFov);
    outAspect = proj[1][1] / proj[0][0];
}
} // namespace

CascadeShadowMap::CascadeShadowMap() = default;

CascadeShadowMap::~CascadeShadowMap() {
    Destroy();
}

CascadeShadowMap::CascadeShadowMap(CascadeShadowMap&& other) noexcept
    : m_resolution(other.m_resolution),
      m_splitCount(other.m_splitCount),
      m_device(other.m_device),
      m_renderTargetRHI(other.m_renderTargetRHI),
      m_lightSpaceBuffer(other.m_lightSpaceBuffer),
      m_frustums(std::move(other.m_frustums)),
      m_farBounds(std::move(other.m_farBounds)),
      m_lightSpace(std::move(other.m_lightSpace)),
      m_cameraNear(other.m_cameraNear),
      m_cameraFar(other.m_cameraFar),
      m_cameraFov(other.m_cameraFov),
      m_cameraAspect(other.m_cameraAspect),
      m_cameraPos(other.m_cameraPos),
      m_view(other.m_view),
      m_projection(other.m_projection) {
    other.m_device = nullptr;
    other.m_renderTargetRHI = nullptr;
    other.m_lightSpaceBuffer = nullptr;
}

CascadeShadowMap& CascadeShadowMap::operator=(CascadeShadowMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_resolution = other.m_resolution;
        m_splitCount = other.m_splitCount;
        m_device = other.m_device;
        m_renderTargetRHI = other.m_renderTargetRHI;
        m_lightSpaceBuffer = other.m_lightSpaceBuffer;
        m_frustums = std::move(other.m_frustums);
        m_farBounds = std::move(other.m_farBounds);
        m_lightSpace = std::move(other.m_lightSpace);
        m_cameraNear = other.m_cameraNear;
        m_cameraFar = other.m_cameraFar;
        m_cameraFov = other.m_cameraFov;
        m_cameraAspect = other.m_cameraAspect;
        m_cameraPos = other.m_cameraPos;
        m_view = other.m_view;
        m_projection = other.m_projection;
        other.m_device = nullptr;
        other.m_renderTargetRHI = nullptr;
        other.m_lightSpaceBuffer = nullptr;
    }
    return *this;
}

bool CascadeShadowMap::Initialize(rhi::IDevice* device, uint32_t resolution, uint32_t splitCount) {
    if (!device || !resolution || !splitCount) return false;
    Destroy();
    m_device = device;
    m_resolution = resolution;
    m_splitCount = std::min(splitCount, 4u);

    m_renderTargetRHI = device->CreateDepthOnlyRenderTargetArray(m_resolution, m_resolution, m_splitCount);
    if (!m_renderTargetRHI) return false;

    const size_t bufferSize = m_splitCount * sizeof(math::Mat4);
    rhi::BufferDesc bd;
    bd.sizeInBytes = bufferSize;
    bd.usage = rhi::BufferUsage::Storage;
    bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_lightSpaceBuffer = device->CreateBuffer(bd);
    if (!m_lightSpaceBuffer) {
        device->DestroyRenderTarget(m_renderTargetRHI);
        m_renderTargetRHI = nullptr;
        return false;
    }

    m_frustums.resize(m_splitCount);
    m_farBounds.resize(m_splitCount);
    m_lightSpace.reserve(m_splitCount);
    return true;
}

void CascadeShadowMap::Update(const math::Vec3& lightDir,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& cameraPos,
                              float nearPlane,
                              float farPlane,
                              float aspectRatio) {
    m_view = view;
    m_projection = projection;
    m_cameraPos = cameraPos;
    m_cameraNear = std::max(nearPlane, 0.05f);
    m_cameraFar = std::max(farPlane, m_cameraNear + 1.0f);
    float fov = 1.0f, aspect = 1.0f;
    deriveFovAspectFromProjection(projection, fov, aspect);
    m_cameraFov = fov;
    m_cameraAspect = (aspectRatio > 0.0f) ? aspectRatio : aspect;

    UpdateFrustums(m_cameraNear, m_cameraFar, m_cameraFov, m_cameraAspect);
    UpdateLightMatrices(lightDir);
}

void CascadeShadowMap::UpdateFrustums(float nearPlane, float farPlane, float fov, float aspectRatio) {
    const float ratio = farPlane / std::max(nearPlane, 1e-5f);

    for (auto& f : m_frustums) {
        f.fov = fov;
        f.ratio = aspectRatio;
    }

    m_frustums[0].near_distance = nearPlane;
    for (uint32_t i = 1; i < m_splitCount; ++i) {
        const float si = static_cast<float>(i) / static_cast<float>(m_splitCount);
        m_frustums[i].near_distance = SPLIT_WEIGHT * (nearPlane * std::pow(ratio, si))
            + (1.0f - SPLIT_WEIGHT) * (nearPlane + (farPlane - nearPlane) * si);
        m_frustums[i - 1].far_distance = m_frustums[i].near_distance * 1.005f;
    }
    m_frustums[m_splitCount - 1].far_distance = farPlane;

    for (uint32_t i = 0; i < m_splitCount; ++i) {
        const float farD = m_frustums[i].far_distance;
        m_farBounds[i] = 0.5f * (-farD * m_projection[2][2] + m_projection[3][2]) / farD + 0.5f;
    }

    const math::Vec3 front = getCameraFront(m_view);
    const math::Vec3 right = getCameraRight(m_view);
    const math::Vec3 up = getCameraUp(m_view);

    for (auto& f : m_frustums) {
        const math::Vec3 fc = m_cameraPos + front * f.far_distance;
        const math::Vec3 nc = m_cameraPos + front * f.near_distance;
        const float near_height = std::tan(f.fov * 0.5f) * f.near_distance;
        const float far_height = std::tan(f.fov * 0.5f) * f.far_distance;
        const float near_width = near_height * f.ratio;
        const float far_width = far_height * f.ratio;

        f.points[0] = nc - up * near_height - right * near_width;
        f.points[1] = nc + up * near_height - right * near_width;
        f.points[2] = nc + up * near_height + right * near_width;
        f.points[3] = nc - up * near_height + right * near_width;
        f.points[4] = fc - up * far_height - right * far_width;
        f.points[5] = fc + up * far_height - right * far_width;
        f.points[6] = fc + up * far_height + right * far_width;
        f.points[7] = fc - up * far_height + right * far_width;
    }
}

void CascadeShadowMap::UpdateLightMatrices(const math::Vec3& lightDir) {
    m_lightSpace.clear();
    glm::vec3 dir(lightDir.x, lightDir.y, lightDir.z);
    float len = glm::length(dir);
    if (len < 1e-6f) {
        dir = glm::vec3(0.5f, -1.0f, 0.2f);
        len = glm::length(dir);
    }
    dir /= len;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(up, dir)) > 0.999f) up = glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::mat4 view = glm::lookAt(-dir, glm::vec3(0.0f), up);

    for (auto& f : m_frustums) {
        glm::vec3 minP(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f);
        glm::vec3 maxP(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), 0.0f);

        for (int j = 0; j < 8; ++j) {
            glm::vec4 p(f.points[j].x, f.points[j].y, f.points[j].z, 1.0f);
            glm::vec4 t = view * p;
            minP.z = std::min(minP.z, t.z);
            maxP.z = std::max(maxP.z, t.z);
        }
        maxP.z += 50.0f;

        const glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -maxP.z, -minP.z);
        const glm::mat4 mvp = proj * view;

        minP.x = minP.y = std::numeric_limits<float>::max();
        maxP.x = maxP.y = std::numeric_limits<float>::lowest();
        for (const auto& pt : f.points) {
            glm::vec4 p(pt.x, pt.y, pt.z, 1.0f);
            glm::vec4 t = mvp * p;
            t.x /= t.w;
            t.y /= t.w;
            minP.x = std::min(minP.x, t.x);
            minP.y = std::min(minP.y, t.y);
            maxP.x = std::max(maxP.x, t.x);
            maxP.y = std::max(maxP.y, t.y);
        }

        const float scaleX = 2.0f / (maxP.x - minP.x);
        const float scaleY = 2.0f / (maxP.y - minP.y);
        const float offsetX = -0.5f * (maxP.x + minP.x) * scaleX;
        const float offsetY = -0.5f * (maxP.y + minP.y) * scaleY;

        glm::mat4 crop(1.0f);
        crop[0][0] = scaleX;
        crop[1][1] = scaleY;
        crop[0][3] = offsetX;
        crop[1][3] = offsetY;
        crop = glm::transpose(crop);
        const glm::mat4 lightSpace = crop * proj * view;

        f.crop[0][0] = lightSpace[0][0]; f.crop[0][1] = lightSpace[0][1]; f.crop[0][2] = lightSpace[0][2]; f.crop[0][3] = lightSpace[0][3];
        f.crop[1][0] = lightSpace[1][0]; f.crop[1][1] = lightSpace[1][1]; f.crop[1][2] = lightSpace[1][2]; f.crop[1][3] = lightSpace[1][3];
        f.crop[2][0] = lightSpace[2][0]; f.crop[2][1] = lightSpace[2][1]; f.crop[2][2] = lightSpace[2][2]; f.crop[2][3] = lightSpace[2][3];
        f.crop[3][0] = lightSpace[3][0]; f.crop[3][1] = lightSpace[3][1]; f.crop[3][2] = lightSpace[3][2]; f.crop[3][3] = lightSpace[3][3];

        m_lightSpace.push_back(f.crop);
    }
}

void CascadeShadowMap::BindForShadowPass(rhi::ICommandList* cmd, uint32_t cascadeIndex) {
    if (!cmd || !m_renderTargetRHI || cascadeIndex >= m_splitCount) return;
    rhi::IRenderTarget* rt = m_renderTargetRHI;
    rt->SetDepthLayer(cascadeIndex);
    rhi::ClearValue cv = {};
    cv.depth = 1.0f;
    rhi::ClearAndSetRenderTargetWithViewport(cmd, rt, 0, 0, m_resolution, m_resolution, cv);
}

void CascadeShadowMap::Unbind(rhi::ICommandList* cmd) {
    if (cmd) cmd->SetRenderTarget(nullptr);
}

const math::Mat4& CascadeShadowMap::GetLightSpaceMatrix(uint32_t cascadeIndex) const {
    static const math::Mat4 identity(1.0f);
    if (cascadeIndex >= m_frustums.size()) return identity;
    return m_frustums[cascadeIndex].crop;
}

rhi::ITexture* CascadeShadowMap::GetDepthTextureRHI() const {
    return m_renderTargetRHI ? m_renderTargetRHI->GetDepthTexture() : nullptr;
}

rhi::IBuffer* CascadeShadowMap::GetLightSpaceBufferRHI() const {
    return m_lightSpaceBuffer;
}

void CascadeShadowMap::MapLightSpaceToBuffer() const {
    if (!m_lightSpaceBuffer || m_lightSpace.size() != m_splitCount) return;
    m_lightSpaceBuffer->Update(m_lightSpace.data(), 0, m_splitCount * sizeof(math::Mat4));
}

void CascadeShadowMap::Destroy() {
    if (m_device) {
        if (m_lightSpaceBuffer) {
            m_device->DestroyBuffer(m_lightSpaceBuffer);
            m_lightSpaceBuffer = nullptr;
        }
        if (m_renderTargetRHI) {
            m_device->DestroyRenderTarget(m_renderTargetRHI);
            m_renderTargetRHI = nullptr;
        }
        m_device = nullptr;
    }
}

} // namespace render
} // namespace eyelib
