#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace physics {

class JoltWorld;

struct JoltRaycastResult {
    bool hit = false;
    eyelib::math::Vec3 point{0.0f};
    eyelib::math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    uint32_t bodyHandle = 0;
};

/**
 * Raycast and AABB overlap using Jolt NarrowPhaseQuery and BroadPhaseQuery.
 * SetWorld() must be called before use.
 */
class JoltQueries {
public:
    JoltQueries() = default;

    void SetWorld(const JoltWorld* world);

    JoltRaycastResult Raycast(const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) const;
    std::vector<uint32_t> QueryAABB(const eyelib::math::AABB& aabb) const;

private:
    const JoltWorld* m_world = nullptr;
};

} // namespace physics
} // namespace eyelib
