#include "eyelib/render/lighting/ClusteredLightGrid.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace eyelib {
namespace render {

ClusteredLightGrid::ClusteredLightGrid() = default;

ClusteredLightGrid::~ClusteredLightGrid() {
    // SSBOs cleaned up by caller
}

ClusteredLightGrid::ClusteredLightGrid(ClusteredLightGrid&& other) noexcept
    : m_screenWidth(other.m_screenWidth), m_screenHeight(other.m_screenHeight),
      m_tileCountX(other.m_tileCountX), m_tileCountY(other.m_tileCountY),
      m_nearPlane(other.m_nearPlane), m_farPlane(other.m_farPlane),
      m_logNear(other.m_logNear), m_logFar(other.m_logFar),
      m_clusterLightIndices(std::move(other.m_clusterLightIndices)),
      m_lightGridHeaders(std::move(other.m_lightGridHeaders)),
      m_lightGridPacked(std::move(other.m_lightGridPacked)),
      m_clusterAABBs(std::move(other.m_clusterAABBs)) {
    other.m_screenWidth = other.m_screenHeight = 0;
    other.m_tileCountX = other.m_tileCountY = 0;
}

ClusteredLightGrid& ClusteredLightGrid::operator=(ClusteredLightGrid&& other) noexcept {
    if (this != &other) {
        m_screenWidth = other.m_screenWidth;
        m_screenHeight = other.m_screenHeight;
        m_tileCountX = other.m_tileCountX;
        m_tileCountY = other.m_tileCountY;
        m_nearPlane = other.m_nearPlane;
        m_farPlane = other.m_farPlane;
        m_logNear = other.m_logNear;
        m_logFar = other.m_logFar;
        m_clusterLightIndices = std::move(other.m_clusterLightIndices);
        m_lightGridHeaders = std::move(other.m_lightGridHeaders);
        m_lightGridPacked = std::move(other.m_lightGridPacked);
        m_clusterAABBs = std::move(other.m_clusterAABBs);
        other.m_screenWidth = other.m_screenHeight = 0;
        other.m_tileCountX = other.m_tileCountY = 0;
    }
    return *this;
}

void ClusteredLightGrid::Initialize(uint32_t screenWidth, uint32_t screenHeight, float nearPlane, float farPlane) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_tileCountX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    m_tileCountY = (screenHeight + TILE_SIZE - 1) / TILE_SIZE;
    m_logNear = std::log(nearPlane);
    m_logFar = std::log(farPlane);
    m_clusterLightIndices.clear();
    m_clusterLightIndices.resize(m_tileCountX * m_tileCountY * DEPTH_SLICES);
    m_clusterAABBs.clear();
    m_clusterAABBs.resize(m_tileCountX * m_tileCountY * DEPTH_SLICES);
    m_lightGridHeaders.clear();
    m_lightGridPacked.clear();
}

void ClusteredLightGrid::Clear() {
    for (auto& cluster : m_clusterLightIndices) {
        cluster.clear();
    }
    m_lightGridHeaders.clear();
    m_lightGridPacked.clear();
}

void ClusteredLightGrid::BuildClusterAABBs(const math::Mat4& view, const math::Mat4& projection) {
    glm::mat4 invViewProj = glm::inverse(glm::mat4(projection) * glm::mat4(view));

    for (uint32_t sliceZ = 0; sliceZ < DEPTH_SLICES; ++sliceZ) {
        float sliceNear = std::exp(m_logNear + (m_logFar - m_logNear) * static_cast<float>(sliceZ) / static_cast<float>(DEPTH_SLICES));
        float sliceFar = std::exp(m_logNear + (m_logFar - m_logNear) * static_cast<float>(sliceZ + 1) / static_cast<float>(DEPTH_SLICES));

        for (uint32_t tileY = 0; tileY < m_tileCountY; ++tileY) {
            for (uint32_t tileX = 0; tileX < m_tileCountX; ++tileX) {
                float tileMinX = static_cast<float>(tileX * TILE_SIZE) / static_cast<float>(m_screenWidth) * 2.0f - 1.0f;
                float tileMaxX = static_cast<float>((tileX + 1) * TILE_SIZE) / static_cast<float>(m_screenWidth) * 2.0f - 1.0f;
                float tileMinY = 1.0f - static_cast<float>((tileY + 1) * TILE_SIZE) / static_cast<float>(m_screenHeight) * 2.0f;
                float tileMaxY = 1.0f - static_cast<float>(tileY * TILE_SIZE) / static_cast<float>(m_screenHeight) * 2.0f;

                // Compute cluster AABB in view space
                // Unproject tile corners at near and far slice depths
                glm::mat4 invProj = glm::inverse(glm::mat4(projection));

                // For perspective: NDC z = (A * viewZ + B) / viewZ
                // Inverse: viewZ = B / (NDC.z - A)
                float A = (m_farPlane + m_nearPlane) / (m_farPlane - m_nearPlane);
                float B = -2.0f * m_farPlane * m_nearPlane / (m_farPlane - m_nearPlane);
                float zNearNDC = (A * sliceNear + B) / sliceNear;
                float zFarNDC = (A * sliceFar + B) / sliceFar;
                zNearNDC = glm::clamp(zNearNDC, -1.0f, 1.0f);
                zFarNDC = glm::clamp(zFarNDC, -1.0f, 1.0f);

                glm::vec4 nearCorners[4] = {
                    invProj * glm::vec4(tileMinX, tileMinY, zNearNDC, 1.0f),
                    invProj * glm::vec4(tileMaxX, tileMinY, zNearNDC, 1.0f),
                    invProj * glm::vec4(tileMaxX, tileMaxY, zNearNDC, 1.0f),
                    invProj * glm::vec4(tileMinX, tileMaxY, zNearNDC, 1.0f)
                };
                glm::vec4 farCorners[4] = {
                    invProj * glm::vec4(tileMinX, tileMinY, zFarNDC, 1.0f),
                    invProj * glm::vec4(tileMaxX, tileMinY, zFarNDC, 1.0f),
                    invProj * glm::vec4(tileMaxX, tileMaxY, zFarNDC, 1.0f),
                    invProj * glm::vec4(tileMinX, tileMaxY, zFarNDC, 1.0f)
                };
                // Divide by w to get view space coordinates
                for (auto& c : nearCorners) c /= c.w;
                for (auto& c : farCorners) c /= c.w;

                uint32_t clusterIdx = (sliceZ * m_tileCountY + tileY) * m_tileCountX + tileX;
                ClusterAABB& aabb = m_clusterAABBs[clusterIdx];

                aabb.min = math::Vec3(std::numeric_limits<float>::max());
                aabb.max = math::Vec3(std::numeric_limits<float>::lowest());

                for (int i = 0; i < 4; ++i) {
                    glm::vec3 near = glm::vec3(nearCorners[i]);
                    glm::vec3 far = glm::vec3(farCorners[i]);
                    aabb.min.x = std::min({aabb.min.x, near.x, far.x});
                    aabb.min.y = std::min({aabb.min.y, near.y, far.y});
                    aabb.min.z = std::min({aabb.min.z, near.z, far.z});
                    aabb.max.x = std::max({aabb.max.x, near.x, far.x});
                    aabb.max.y = std::max({aabb.max.y, near.y, far.y});
                    aabb.max.z = std::max({aabb.max.z, near.z, far.z});
                }

                // AABB is now in view space (after invProj and divide by w)
            }
        }
    }
}

bool ClusteredLightGrid::SphereIntersectsAABB(const math::Vec3& sphereCenter, float sphereRadius, const ClusterAABB& aabb) const {
    math::Vec3 closest(
        std::max(aabb.min.x, std::min(sphereCenter.x, aabb.max.x)),
        std::max(aabb.min.y, std::min(sphereCenter.y, aabb.max.y)),
        std::max(aabb.min.z, std::min(sphereCenter.z, aabb.max.z))
    );
    math::Vec3 diff = sphereCenter - closest;
    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    return distSq <= sphereRadius * sphereRadius;
}

bool ClusteredLightGrid::ConeIntersectsAABB(const math::Vec3& conePos, const math::Vec3& coneDir, float coneRange,
                                            float innerCos, float outerCos, const ClusterAABB& aabb) const {
    (void)innerCos;
    (void)outerCos;
    math::Vec3 aabbCenter((aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f, (aabb.min.z + aabb.max.z) * 0.5f);
    math::Vec3 toAABB = aabbCenter - conePos;
    float dist = std::sqrt(toAABB.x * toAABB.x + toAABB.y * toAABB.y + toAABB.z * toAABB.z);
    if (dist > coneRange) return false;
    math::Vec3 dirNorm(coneDir.x, coneDir.y, coneDir.z);
    float len = std::sqrt(dirNorm.x * dirNorm.x + dirNorm.y * dirNorm.y + dirNorm.z * dirNorm.z);
    if (len > 0.001f) {
        dirNorm.x /= len; dirNorm.y /= len; dirNorm.z /= len;
    }
    float cosAngle = (toAABB.x * dirNorm.x + toAABB.y * dirNorm.y + toAABB.z * dirNorm.z) / dist;
    return cosAngle >= outerCos;
}

void ClusteredLightGrid::CullLightsForCluster(uint32_t tileX, uint32_t tileY, uint32_t sliceZ,
                                              const std::vector<PointLight>& pointLights,
                                              const std::vector<SpotLight>& spotLights,
                                              const math::Mat4& view,
                                              const math::Vec3& cameraPos) {
    (void)cameraPos;
    uint32_t clusterIdx = (sliceZ * m_tileCountY + tileY) * m_tileCountX + tileX;
    auto& lights = m_clusterLightIndices[clusterIdx];
    lights.clear();
    const ClusterAABB& aabb = m_clusterAABBs[clusterIdx];

    glm::mat4 viewMat(view);
    uint32_t lightIdx = 0;
    for (const auto& pl : pointLights) {
        if (lights.size() >= MAX_LIGHTS_PER_CLUSTER) break;
        glm::vec4 worldPos(pl.position.x, pl.position.y, pl.position.z, 1.0f);
        glm::vec4 viewPos = viewMat * worldPos;
        math::Vec3 viewPosVec3(viewPos.x, viewPos.y, viewPos.z);
        if (SphereIntersectsAABB(viewPosVec3, pl.radius, aabb)) {
            lights.push_back(lightIdx);
        }
        ++lightIdx;
    }

    uint32_t spotBase = static_cast<uint32_t>(pointLights.size());
    lightIdx = 0;
    for (const auto& sl : spotLights) {
        if (lights.size() >= MAX_LIGHTS_PER_CLUSTER) break;
        glm::vec4 worldPos(sl.position.x, sl.position.y, sl.position.z, 1.0f);
        glm::vec4 viewPos = viewMat * worldPos;
        math::Vec3 viewPosVec3(viewPos.x, viewPos.y, viewPos.z);
        glm::mat3 viewMat3(view);
        glm::vec3 worldDir(sl.direction.x, sl.direction.y, sl.direction.z);
        glm::vec3 viewDir = viewMat3 * worldDir;
        math::Vec3 viewDirVec3(viewDir.x, viewDir.y, viewDir.z);
        if (ConeIntersectsAABB(viewPosVec3, viewDirVec3, sl.range, sl.innerConeCos, sl.outerConeCos, aabb)) {
            lights.push_back(spotBase + lightIdx);
        }
        ++lightIdx;
    }
}

void ClusteredLightGrid::BuildLightGrid(const std::vector<PointLight>& pointLights,
                                        const std::vector<SpotLight>& spotLights,
                                        const math::Mat4& view,
                                        const math::Mat4& projection,
                                        const math::Vec3& cameraPos) {
    Clear();
    BuildClusterAABBs(view, projection);

    for (uint32_t sliceZ = 0; sliceZ < DEPTH_SLICES; ++sliceZ) {
        for (uint32_t tileY = 0; tileY < m_tileCountY; ++tileY) {
            for (uint32_t tileX = 0; tileX < m_tileCountX; ++tileX) {
                CullLightsForCluster(tileX, tileY, sliceZ, pointLights, spotLights, view, cameraPos);
            }
        }
    }

    uint32_t totalClusters = m_tileCountX * m_tileCountY * DEPTH_SLICES;
    m_lightGridHeaders.resize(totalClusters * 2u);
    m_lightGridPacked.clear();
    uint32_t runningOffset = 0u;
    for (uint32_t i = 0; i < totalClusters && i < m_clusterLightIndices.size(); ++i) {
        const auto& cluster = m_clusterLightIndices[i];
        uint32_t count = static_cast<uint32_t>(std::min(cluster.size(), static_cast<size_t>(MAX_LIGHTS_PER_CLUSTER)));
        m_lightGridHeaders[i * 2u] = runningOffset;
        m_lightGridHeaders[i * 2u + 1u] = count;
        for (uint32_t j = 0; j < count; ++j)
            m_lightGridPacked.push_back(cluster[j]);
        runningOffset += count;
    }
}

size_t ClusteredLightGrid::GetLightIndexDataSize() const {
    uint32_t totalClusters = m_tileCountX * m_tileCountY * DEPTH_SLICES;
    if (totalClusters == 0) return 0u;
    const size_t headerUints = 1u + totalClusters * 2u;
    const size_t indexUints = totalClusters * MAX_LIGHTS_PER_CLUSTER;
    return (headerUints + indexUints) * sizeof(uint32_t);
}

const std::vector<uint32_t>& ClusteredLightGrid::GetClusterLights(uint32_t tileX, uint32_t tileY, uint32_t sliceZ) const {
    static const std::vector<uint32_t> empty;
    if (tileX >= m_tileCountX || tileY >= m_tileCountY || sliceZ >= DEPTH_SLICES) return empty;
    uint32_t idx = (sliceZ * m_tileCountY + tileY) * m_tileCountX + tileX;
    if (idx >= m_clusterLightIndices.size()) return empty;
    return m_clusterLightIndices[idx];
}

void ClusteredLightGrid::UploadToGPU(rhi::IBuffer* lightIndexBuffer) {
    if (!lightIndexBuffer) return;
    const size_t required = GetLightIndexDataSize();
    if (required == 0 || required > lightIndexBuffer->GetSize()) return;

    uint32_t totalClusters = m_tileCountX * m_tileCountY * DEPTH_SLICES;
    const size_t headerBytes = (1u + totalClusters * 2u) * sizeof(uint32_t);
    std::vector<uint32_t> staging(1u + totalClusters * 2u + totalClusters * MAX_LIGHTS_PER_CLUSTER, 0u);
    staging[0] = 0u;
    for (uint32_t i = 0; i < totalClusters && i * 2u + 1u < m_lightGridHeaders.size(); ++i) {
        staging[1u + i * 2u] = m_lightGridHeaders[i * 2u];
        staging[1u + i * 2u + 1u] = m_lightGridHeaders[i * 2u + 1u];
    }
    const size_t packedBytes = m_lightGridPacked.size() * sizeof(uint32_t);
    if (packedBytes > 0 && headerBytes + packedBytes <= required) {
        std::memcpy(staging.data() + 1u + totalClusters * 2u, m_lightGridPacked.data(), packedBytes);
    }
    void* ptr = lightIndexBuffer->Map(0, required);
    if (ptr) {
        std::memcpy(ptr, staging.data(), headerBytes);
        if (packedBytes > 0)
            std::memcpy(static_cast<char*>(ptr) + headerBytes, m_lightGridPacked.data(), packedBytes);
        lightIndexBuffer->Unmap();
    }
}

} // namespace render
} // namespace eyelib
