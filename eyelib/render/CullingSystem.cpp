#include "eyelib/render/CullingSystem.h"
#include "eyelib/math/Sphere.h"

namespace eyelib {
namespace render {

int CullingSystem::CullAABB(const math::AABB& aabb, const math::Frustum& frustum) {
    return frustum.Contains(aabb);
}

bool CullingSystem::CullPoint(const math::Vec3& point, const math::Frustum& frustum) {
    return frustum.Contains(point);
}

bool CullingSystem::CullSphere(const math::Vec3& center, float radius, const math::Frustum& frustum) {
    return frustum.Contains(math::Sphere{center, radius});
}

size_t CullingSystem::CullFrustum(const std::vector<math::AABB>& aabbs, const math::Frustum& frustum,
                                  std::vector<int>& results) {
    results.clear();
    results.reserve(aabbs.size());
    size_t visibleCount = 0;

    for (const auto& aabb : aabbs) {
        int result = frustum.Contains(aabb);
        results.push_back(result);
        if (result > 0) {
            visibleCount++;
        }
    }

    return visibleCount;
}

std::vector<size_t> CullingSystem::GetVisibleIndices(const std::vector<math::AABB>& aabbs,
                                                      const math::Frustum& frustum) {
    std::vector<size_t> visible;
    visible.reserve(aabbs.size());

    for (size_t i = 0; i < aabbs.size(); ++i) {
        if (frustum.Contains(aabbs[i]) > 0) {
            visible.push_back(i);
        }
    }

    return visible;
}

} // namespace render
} // namespace eyelib
