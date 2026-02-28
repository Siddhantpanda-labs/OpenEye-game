#include "NarrowPhaseSystem.h"

#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/WorldAABB.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace eyelib {
namespace world {
namespace systems {

struct NarrowPhaseResult {
    math::Vec3 normal{0.f, 1.f, 0.f};
    float depth = 0.f;
    math::Vec3 contactPoint{0.f, 0.f, 0.f};
    bool valid = false;
};

static math::Vec3 ClosestPointOnAABB(const math::Vec3& p, const components::WorldAABB& box) {
    return math::Vec3(
        std::max(box.min.x, std::min(p.x, box.max.x)),
        std::max(box.min.y, std::min(p.y, box.max.y)),
        std::max(box.min.z, std::min(p.z, box.max.z)));
}

static NarrowPhaseResult SphereSphere(const math::Vec3& centerA, float radiusA, const math::Vec3& centerB, float radiusB) {
    NarrowPhaseResult r;
    math::Vec3 delta = centerB - centerA;
    float dist = glm::length(delta);
    if (dist < 1e-6f) {
        r.normal = math::Vec3(1.f, 0.f, 0.f);
        r.depth = radiusA + radiusB;
        r.contactPoint = centerA;
        r.valid = true;
        return r;
    }
    r.normal = delta / dist;
    r.depth = radiusA + radiusB - dist;
    r.contactPoint = centerA + r.normal * radiusA;
    r.valid = r.depth > 0.f;
    return r;
}

static NarrowPhaseResult SphereBox(const math::Vec3& sphereCenter, float sphereRadius, const components::WorldAABB& box) {
    NarrowPhaseResult r;
    math::Vec3 closest = ClosestPointOnAABB(sphereCenter, box);
    r.contactPoint = closest;
    math::Vec3 delta = sphereCenter - closest;
    float len = glm::length(delta);
    if (len < 1e-6f) {
        math::Vec3 boxCenter = (box.min + box.max) * 0.5f;
        delta = sphereCenter - boxCenter;
        len = glm::length(delta);
        if (len < 1e-6f) { r.normal = math::Vec3(1.f, 0.f, 0.f); r.depth = sphereRadius; r.valid = true; return r; }
        r.normal = delta / len;
    } else {
        r.normal = delta / len;
    }
    r.depth = sphereRadius - len;
    r.valid = r.depth > 0.f;
    return r;
}

static NarrowPhaseResult ContactFromAABB(const components::WorldAABB& a, const components::WorldAABB& b) {
    NarrowPhaseResult r;
    float overlapX = std::min(a.max.x - b.min.x, b.max.x - a.min.x);
    float overlapY = std::min(a.max.y - b.min.y, b.max.y - a.min.y);
    float overlapZ = std::min(a.max.z - b.min.z, b.max.z - a.min.z);
    math::Vec3 centerA = (a.min + a.max) * 0.5f;
    math::Vec3 centerB = (b.min + b.max) * 0.5f;
    float dx = centerB.x - centerA.x, dy = centerB.y - centerA.y, dz = centerB.z - centerA.z;
    if (overlapX <= overlapY && overlapX <= overlapZ) {
        r.normal = math::Vec3(dx >= 0.f ? 1.f : -1.f, 0.f, 0.f);
        r.depth = overlapX;
        r.contactPoint.x = (std::max(a.min.x, b.min.x) + std::min(a.max.x, b.max.x)) * 0.5f;
        r.contactPoint.y = (centerA.y + centerB.y) * 0.5f;
        r.contactPoint.z = (centerA.z + centerB.z) * 0.5f;
    } else if (overlapY <= overlapZ) {
        r.normal = math::Vec3(0.f, dy >= 0.f ? 1.f : -1.f, 0.f);
        r.depth = overlapY;
        r.contactPoint.x = (centerA.x + centerB.x) * 0.5f;
        r.contactPoint.y = (std::max(a.min.y, b.min.y) + std::min(a.max.y, b.max.y)) * 0.5f;
        r.contactPoint.z = (centerA.z + centerB.z) * 0.5f;
    } else {
        r.normal = math::Vec3(0.f, 0.f, dz >= 0.f ? 1.f : -1.f);
        r.depth = overlapZ;
        r.contactPoint.x = (centerA.x + centerB.x) * 0.5f;
        r.contactPoint.y = (centerA.y + centerB.y) * 0.5f;
        r.contactPoint.z = (std::max(a.min.z, b.min.z) + std::min(a.max.z, b.max.z)) * 0.5f;
    }
    r.valid = true;
    return r;
}

static math::Vec3 WorldSphereCenter(const math::Mat4& world, const math::Vec3& localCenter) {
    glm::vec4 p(localCenter.x, localCenter.y, localCenter.z, 1.f);
    glm::vec4 w = world * p;
    return math::Vec3(w.x, w.y, w.z);
}

static float WorldSphereRadius(float localRadius, const math::Vec3& scale) {
    float s = std::max({scale.x, scale.y, scale.z});
    return localRadius * s;
}

static void WorldCapsuleSegment(const math::Mat4& world, const components::Collider& c, math::Vec3& p1, math::Vec3& p2, float& worldRadius, const math::Vec3& scale) {
    float h = c.height * 0.5f;
    glm::vec4 local1(c.center.x, c.center.y - h, c.center.z, 1.f);
    glm::vec4 local2(c.center.x, c.center.y + h, c.center.z, 1.f);
    glm::vec4 w1 = world * local1, w2 = world * local2;
    p1 = math::Vec3(w1.x, w1.y, w1.z);
    p2 = math::Vec3(w2.x, w2.y, w2.z);
    worldRadius = c.radius * std::max({scale.x, scale.y, scale.z});
}

static math::Vec3 ClosestPointOnSegment(const math::Vec3& s1, const math::Vec3& s2, const math::Vec3& p) {
    math::Vec3 d = s2 - s1;
    float len2 = glm::dot(d, d);
    if (len2 < 1e-12f) return s1;
    float t = glm::dot(p - s1, d) / len2;
    t = std::max(0.f, std::min(1.f, t));
    return s1 + t * d;
}

static void ClosestPointSegmentSegment(const math::Vec3& pa1, const math::Vec3& pa2, const math::Vec3& pb1, const math::Vec3& pb2,
    math::Vec3& outA, math::Vec3& outB) {
    math::Vec3 d1 = pa2 - pa1, d2 = pb2 - pb1, r = pa1 - pb1;
    float a = glm::dot(d1, d1), b = glm::dot(d1, d2), c = glm::dot(d2, d2), d = glm::dot(d1, r), e = glm::dot(d2, r);
    float det = a * c - b * b;
    float t, s;
    if (std::fabs(det) < 1e-9f) {
        t = 0.f;
        s = (c > 1e-9f) ? std::max(0.f, std::min(1.f, -e / c)) : 0.f;
    } else {
        t = (b * e - c * d) / det;
        s = (a * e - b * d) / det;
        t = std::max(0.f, std::min(1.f, t));
        s = std::max(0.f, std::min(1.f, s));
    }
    outA = pa1 + t * d1;
    outB = pb1 + s * d2;
}

static NarrowPhaseResult CapsuleSphere(const math::Vec3& capP1, const math::Vec3& capP2, float capRadius,
    const math::Vec3& sphereCenter, float sphereRadius) {
    NarrowPhaseResult r;
    math::Vec3 closest = ClosestPointOnSegment(capP1, capP2, sphereCenter);
    math::Vec3 delta = sphereCenter - closest;
    float dist = glm::length(delta);
    if (dist < 1e-6f) {
        r.normal = math::Vec3(1.f, 0.f, 0.f);
        r.depth = capRadius + sphereRadius;
        r.contactPoint = closest + math::Vec3(capRadius, 0.f, 0.f);
        r.valid = true;
        return r;
    }
    r.normal = delta / dist;
    r.depth = capRadius + sphereRadius - dist;
    r.contactPoint = closest + r.normal * capRadius;
    r.valid = r.depth > 0.f;
    return r;
}

static NarrowPhaseResult CapsuleCapsule(const math::Vec3& p1a, const math::Vec3& p2a, float rA,
    const math::Vec3& p1b, const math::Vec3& p2b, float rB) {
    NarrowPhaseResult r;
    math::Vec3 closestA, closestB;
    ClosestPointSegmentSegment(p1a, p2a, p1b, p2b, closestA, closestB);
    math::Vec3 delta = closestB - closestA;
    float dist = glm::length(delta);
    if (dist < 1e-6f) {
        r.normal = math::Vec3(1.f, 0.f, 0.f);
        r.depth = rA + rB;
        r.contactPoint = (closestA + closestB) * 0.5f;
        r.valid = true;
        return r;
    }
    r.normal = delta / dist;
    r.depth = rA + rB - dist;
    r.contactPoint = (closestA + closestB) * 0.5f;
    r.valid = r.depth > 0.f;
    return r;
}

static NarrowPhaseResult GeometryNarrowPhase(
    const components::Collider* collA, const components::WorldAABB& aabbA, const math::Mat4& worldA, const math::Vec3& scaleA,
    const components::Collider* collB, const components::WorldAABB& aabbB, const math::Mat4& worldB, const math::Vec3& scaleB)
{
    using T = components::Collider::Type;
    bool hasA = collA != nullptr, hasB = collB != nullptr;
    T typeA = hasA ? collA->type : T::Box;
    T typeB = hasB ? collB->type : T::Box;

    if (typeA == T::Sphere && typeB == T::Sphere) {
        math::Vec3 cA = WorldSphereCenter(worldA, collA->center), cB = WorldSphereCenter(worldB, collB->center);
        float rA = WorldSphereRadius(collA->radius, scaleA), rB = WorldSphereRadius(collB->radius, scaleB);
        return SphereSphere(cA, rA, cB, rB);
    }
    if (typeA == T::Sphere && (typeB == T::Box || typeB == T::Capsule)) {
        math::Vec3 cA = WorldSphereCenter(worldA, collA->center);
        float rA = WorldSphereRadius(collA->radius, scaleA);
        return SphereBox(cA, rA, aabbB);
    }
    if ((typeA == T::Box || typeA == T::Capsule) && typeB == T::Sphere) {
        math::Vec3 cB = WorldSphereCenter(worldB, collB->center);
        float rB = WorldSphereRadius(collB->radius, scaleB);
        NarrowPhaseResult r = SphereBox(cB, rB, aabbA);
        r.normal = -r.normal;
        return r;
    }
    if (typeA == T::Capsule && typeB == T::Sphere) {
        math::Vec3 capP1, capP2;
        float capR;
        WorldCapsuleSegment(worldA, *collA, capP1, capP2, capR, scaleA);
        math::Vec3 cB = WorldSphereCenter(worldB, collB->center);
        float rB = WorldSphereRadius(collB->radius, scaleB);
        return CapsuleSphere(capP1, capP2, capR, cB, rB);
    }
    if (typeA == T::Sphere && typeB == T::Capsule) {
        math::Vec3 capP1, capP2;
        float capR;
        WorldCapsuleSegment(worldB, *collB, capP1, capP2, capR, scaleB);
        math::Vec3 cA = WorldSphereCenter(worldA, collA->center);
        float rA = WorldSphereRadius(collA->radius, scaleA);
        NarrowPhaseResult r = CapsuleSphere(capP1, capP2, capR, cA, rA);
        r.normal = -r.normal;
        return r;
    }
    if (typeA == T::Capsule && typeB == T::Capsule) {
        math::Vec3 p1a, p2a, p1b, p2b;
        float rA, rB;
        WorldCapsuleSegment(worldA, *collA, p1a, p2a, rA, scaleA);
        WorldCapsuleSegment(worldB, *collB, p1b, p2b, rB, scaleB);
        return CapsuleCapsule(p1a, p2a, rA, p1b, p2b, rB);
    }
    return NarrowPhaseResult{};
}

void NarrowPhaseSystem::Update(WorldState& state,
                               const std::vector<std::pair<EntityId, EntityId>>& pairs,
                               std::vector<ContactEvent>& outContacts) const {
    outContacts.clear();
    const auto& worldAabbs = state.Components<components::WorldAABB>();
    const auto& colliders = state.Components<components::Collider>();
    const auto& transformStore = state.GetTransformStore();

    for (const auto& [entityA, entityB] : pairs) {
        const components::WorldAABB* aabbA = worldAabbs.TryGet(entityA);
        const components::WorldAABB* aabbB = worldAabbs.TryGet(entityB);
        if (!aabbA || !aabbB) continue;

        const components::Collider* collA = colliders.TryGet(entityA);
        const components::Collider* collB = colliders.TryGet(entityB);
        const auto* refA = transformStore.TryGet(entityA);
        const auto* refB = transformStore.TryGet(entityB);
        math::Mat4 worldA = transformStore.GetWorldMatrix(entityA);
        math::Mat4 worldB = transformStore.GetWorldMatrix(entityB);
        math::Vec3 scaleA = refA ? refA->Scale() : math::Vec3(1.f);
        math::Vec3 scaleB = refB ? refB->Scale() : math::Vec3(1.f);

        NarrowPhaseResult narrow = GeometryNarrowPhase(collA, *aabbA, worldA, scaleA, collB, *aabbB, worldB, scaleB);
        NarrowPhaseResult r = narrow.valid ? narrow : ContactFromAABB(*aabbA, *aabbB);

        if (r.depth <= 0.f) continue;

        ContactEvent evt;
        evt.entityA = entityA;
        evt.entityB = entityB;
        evt.normal = r.normal;
        evt.penetrationDepth = r.depth;
        evt.contactPoint = r.contactPoint;
        outContacts.push_back(evt);
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
