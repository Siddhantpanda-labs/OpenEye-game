#pragma once

#include "eyelib/math/Frustum.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"
#include <vector>
#include <cstddef>

namespace eyelib {
namespace render {

/**
 * @brief Culling system for frustum culling.
 * Tests AABBs against view frustum to determine visibility.
 */
class CullingSystem {
public:
    /**
     * @brief Test if an AABB is visible within the frustum.
     * @param aabb Axis-aligned bounding box
     * @param frustum View frustum
     * @return 0 = outside, 1 = intersect, 2 = inside
     */
    static int CullAABB(const math::AABB& aabb, const math::Frustum& frustum);

    /**
     * @brief Test if a point is visible within the frustum.
     */
    static bool CullPoint(const math::Vec3& point, const math::Frustum& frustum);

    /**
     * @brief Test if a sphere is visible within the frustum.
     */
    static bool CullSphere(const math::Vec3& center, float radius, const math::Frustum& frustum);

    /**
     * @brief Cull a list of AABBs against frustum.
     * @param aabbs Input AABBs
     * @param frustum View frustum
     * @param results Output visibility results (0=outside, 1=intersect, 2=inside)
     * @return Number of visible objects (result > 0)
     */
    static size_t CullFrustum(const std::vector<math::AABB>& aabbs, const math::Frustum& frustum,
                              std::vector<int>& results);

    /**
     * @brief Cull a list of AABBs and return only visible indices.
     * @param aabbs Input AABBs
     * @param frustum View frustum
     * @return Indices of visible AABBs (result > 0)
     */
    static std::vector<size_t> GetVisibleIndices(const std::vector<math::AABB>& aabbs,
                                                 const math::Frustum& frustum);
};

} // namespace render
} // namespace eyelib
