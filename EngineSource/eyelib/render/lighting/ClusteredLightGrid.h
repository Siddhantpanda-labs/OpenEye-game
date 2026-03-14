#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

/** 3D AABB in view space */
struct ClusterAABB {
    math::Vec3 min{-1, -1, -1};
    math::Vec3 max{1, 1, 1};
};

/** Clustered light grid: per-cluster (tile × depth slice) light lists */
class ClusteredLightGrid {
public:
    static constexpr uint32_t TILE_SIZE = 16;  // 16x16 pixels per tile
    static constexpr uint32_t DEPTH_SLICES = 32;  // Depth slices (3D clusters); AAA typical
    /** Max lights per cluster (cap to avoid stalls). Indices stored as uint. */
    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 64u;

    ClusteredLightGrid();
    ~ClusteredLightGrid();

    ClusteredLightGrid(const ClusteredLightGrid&) = delete;
    ClusteredLightGrid& operator=(const ClusteredLightGrid&) = delete;
    ClusteredLightGrid(ClusteredLightGrid&& other) noexcept;
    ClusteredLightGrid& operator=(ClusteredLightGrid&& other) noexcept;

    /** Initialize for screen size and camera. Builds cluster grid dimensions. */
    void Initialize(uint32_t screenWidth, uint32_t screenHeight, float nearPlane, float farPlane);

    /** Clear all light lists */
    void Clear();

    /** Build clustered light grid from point/spot lights and camera frustum */
    void BuildLightGrid(const std::vector<PointLight>& pointLights,
                        const std::vector<SpotLight>& spotLights,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos);

    /** Get light indices for a cluster (tileX, tileY, sliceZ) */
    const std::vector<uint32_t>& GetClusterLights(uint32_t tileX, uint32_t tileY, uint32_t sliceZ) const;

    /** Required byte size for compact light index buffer: 1 (counter) + totalClusters*2 (headers) + totalClusters*MAX_LIGHTS_PER_CLUSTER (indices), in uints. */
    size_t GetLightIndexDataSize() const;
    /** RHI path: upload compact (offset+count) light indices. Buffer must be at least GetLightIndexDataSize() bytes. */
    void UploadToGPU(rhi::IBuffer* lightIndexBuffer);

    uint32_t GetTileCountX() const { return m_tileCountX; }
    uint32_t GetTileCountY() const { return m_tileCountY; }
    uint32_t GetSliceCount() const { return DEPTH_SLICES; }
    uint32_t GetTotalClusters() const { return m_tileCountX * m_tileCountY * DEPTH_SLICES; }
    float GetLogNear() const { return m_logNear; }
    float GetLogFar() const { return m_logFar; }
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }

private:
    uint32_t m_screenWidth = 0, m_screenHeight = 0;
    uint32_t m_tileCountX = 0, m_tileCountY = 0;
    float m_nearPlane = 0.1f, m_farPlane = 1000.0f;
    float m_logNear = 0.0f, m_logFar = 0.0f;

    /** Per-cluster light indices: [clusterIndex] = vector of light indices (capped at MAX_LIGHTS_PER_CLUSTER) */
    std::vector<std::vector<uint32_t>> m_clusterLightIndices;

    /** Compact layout: headers (offset, count) per cluster, then packed indices. Filled in BuildLightGrid for UploadToGPU. */
    std::vector<uint32_t> m_lightGridHeaders;
    std::vector<uint32_t> m_lightGridPacked;

    /** Cluster AABBs in view space: [clusterIndex] = AABB */
    std::vector<ClusterAABB> m_clusterAABBs;

    /** Build cluster AABBs for all clusters */
    void BuildClusterAABBs(const math::Mat4& view, const math::Mat4& projection);

    /** Cull lights for a specific 3D cluster */
    void CullLightsForCluster(uint32_t tileX, uint32_t tileY, uint32_t sliceZ,
                             const std::vector<PointLight>& pointLights,
                             const std::vector<SpotLight>& spotLights,
                             const math::Mat4& view,
                             const math::Vec3& cameraPos);

    /** Test if sphere intersects AABB */
    bool SphereIntersectsAABB(const math::Vec3& sphereCenter, float sphereRadius, const ClusterAABB& aabb) const;

    /** Test if cone intersects AABB (simplified: sphere approximation) */
    bool ConeIntersectsAABB(const math::Vec3& conePos, const math::Vec3& coneDir, float coneRange,
                            float innerCos, float outerCos, const ClusterAABB& aabb) const;
};

} // namespace render
} // namespace eyelib
