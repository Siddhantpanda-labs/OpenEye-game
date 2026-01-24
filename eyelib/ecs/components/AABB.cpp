#include "AABB.h"

#include <algorithm>
#include <limits>

namespace eyelib {
namespace ecs {
namespace components {

AABB AABB::Transform(const math::Mat4& matrix) const {
    const math::Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z}
    };

    math::Vec3 outMin( std::numeric_limits<float>::max());
    math::Vec3 outMax(-std::numeric_limits<float>::max());

    for (const auto& c : corners) {
        math::Vec4 h = matrix * math::Vec4(c, 1.0f);
        math::Vec3 p(h);
        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }

    AABB out;
    out.min = outMin;
    out.max = outMax;
    out.isVisible = isVisible;
    return out;
}

} // namespace components
} // namespace ecs
} // namespace eyelib
