#include "JoltQueries.h"
#include "JoltWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Body/BodyID.h>

JPH_SUPPRESS_WARNINGS

namespace eyelib {
namespace physics {

namespace {

// Collector that collects BodyIDs from CollideAABox into a vector of handles.
class AABBCollector : public JPH::CollideShapeBodyCollector {
public:
    explicit AABBCollector(std::vector<uint32_t>& out) : m_out(out) {}
    void AddHit(const JPH::BodyID& inResult) override {
        m_out.push_back(inResult.GetIndexAndSequenceNumber());
    }
private:
    std::vector<uint32_t>& m_out;
};

} // namespace

void JoltQueries::SetWorld(const JoltWorld* world) {
    m_world = world;
}

JoltRaycastResult JoltQueries::Raycast(const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) const {
    JoltRaycastResult out;
    if (!m_world) return out;
    const void* np = m_world->GetNarrowPhaseQuery();
    if (!np) return out;
    const auto* nq = static_cast<const JPH::NarrowPhaseQuery*>(np);

    JPH::RVec3 origin(from.x, from.y, from.z);
    JPH::Vec3 dir(to.x - from.x, to.y - from.y, to.z - from.z);
    float len = dir.Length();
    if (len < 1e-6f) return out;
    JPH::RRayCast ray(origin, dir);

    JPH::RayCastResult hit;
    hit.Reset();
    if (!nq->CastRay(ray, hit)) return out;

    out.hit = true;
    out.bodyHandle = hit.mBodyID.GetIndexAndSequenceNumber();
    out.distance = hit.mFraction * len;
    out.point.x = from.x + hit.mFraction * (to.x - from.x);
    out.point.y = from.y + hit.mFraction * (to.y - from.y);
    out.point.z = from.z + hit.mFraction * (to.z - from.z);
    out.normal = eyelib::math::Vec3(0.0f, 1.0f, 0.0f);
    return out;
}

std::vector<uint32_t> JoltQueries::QueryAABB(const eyelib::math::AABB& aabb) const {
    std::vector<uint32_t> out;
    if (!m_world) return out;
    const void* bp = m_world->GetBroadPhaseQuery();
    if (!bp) return out;
    const auto* bq = static_cast<const JPH::BroadPhaseQuery*>(bp);

    JPH::AABox box(
        JPH::Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
        JPH::Vec3(aabb.max.x, aabb.max.y, aabb.max.z)
    );
    AABBCollector collector(out);
    bq->CollideAABox(box, collector);
    return out;
}

} // namespace physics
} // namespace eyelib
