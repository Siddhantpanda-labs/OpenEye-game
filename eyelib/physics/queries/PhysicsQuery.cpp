#include "eyelib/physics/queries/PhysicsQuery.h"
#include "eyelib/core/Log.h"

#include <btBulletDynamicsCommon.h>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
math::Vec3 ToGlm(const btVector3& v) { return math::Vec3(v.x(), v.y(), v.z()); }
btQuaternion ToBt(const math::Quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }

struct OverlapCallback : public btCollisionWorld::ContactResultCallback {
    PhysicsWorld* world = nullptr;
    std::vector<OverlapResult>* results = nullptr;

    btScalar addSingleResult(btManifoldPoint& cp,
                             const btCollisionObjectWrapper* colObj0Wrap, int, int,
                             const btCollisionObjectWrapper* colObj1Wrap, int, int) override {
        if (!world || !results) return 0;
        const btCollisionObject* other = colObj1Wrap ? colObj1Wrap->getCollisionObject() : nullptr;
        RigidBodyHandle body = world->GetHandleFromCollisionObject(other);
        if (body == 0) return 0;
        OverlapResult r;
        r.body = body;
        r.contactPoint = ToGlm(cp.m_positionWorldOnB);
        r.normal = ToGlm(cp.m_normalWorldOnB);
        results->push_back(r);
        return 0;
    }
};
} // namespace

RaycastResult PhysicsQuery::Raycast(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to) {
    RaycastResult out;
    if (!world) return out;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return out;

    btCollisionWorld::ClosestRayResultCallback cb(ToBt(from), ToBt(to));
    w->rayTest(ToBt(from), ToBt(to), cb);
    if (cb.hasHit()) {
        out.hit = true;
        out.point = ToGlm(cb.m_hitPointWorld);
        out.normal = ToGlm(cb.m_hitNormalWorld);
        out.distance = glm::length(out.point - from);
        out.body = world->GetHandleFromCollisionObject(cb.m_collisionObject);
    }
    return out;
}

std::vector<RaycastResult> PhysicsQuery::RaycastAll(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to) {
    std::vector<RaycastResult> results;
    if (!world) return results;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return results;

    btCollisionWorld::AllHitsRayResultCallback cb(ToBt(from), ToBt(to));
    w->rayTest(ToBt(from), ToBt(to), cb);
    if (!cb.hasHit()) return results;

    for (int i = 0; i < cb.m_hitPointWorld.size(); ++i) {
        RaycastResult r;
        r.hit = true;
        r.point = ToGlm(cb.m_hitPointWorld[i]);
        r.normal = ToGlm(cb.m_hitNormalWorld[i]);
        r.distance = glm::length(r.point - from);
        r.body = world->GetHandleFromCollisionObject(cb.m_collisionObjects[i]);
        results.push_back(r);
    }
    return results;
}

RaycastResult PhysicsQuery::RaycastClosest(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to) {
    return Raycast(world, from, to);
}

SweepResult PhysicsQuery::SweepTest(PhysicsWorld* world, CollisionShapeHandle shape,
                                    const math::Vec3& from, const math::Vec3& to,
                                    const math::Quat& rotation) {
    SweepResult out;
    if (!world) return out;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return out;
    btCollisionShape* s = CollisionShapeFactory::GetShape(shape);
    auto* convex = dynamic_cast<btConvexShape*>(s);
    if (!convex) {
        core::Log::Warn("PhysicsQuery::SweepTest: shape is not convex");
        return out;
    }
    btTransform tFrom, tTo;
    tFrom.setIdentity();
    tTo.setIdentity();
    tFrom.setOrigin(ToBt(from));
    tTo.setOrigin(ToBt(to));
    tFrom.setRotation(ToBt(rotation));
    tTo.setRotation(ToBt(rotation));

    btCollisionWorld::ClosestConvexResultCallback cb(ToBt(from), ToBt(to));
    w->convexSweepTest(convex, tFrom, tTo, cb);
    if (cb.hasHit()) {
        out.hit = true;
        out.point = ToGlm(cb.m_hitPointWorld);
        out.normal = ToGlm(cb.m_hitNormalWorld);
        out.distance = glm::length(out.point - from);
        out.fraction = cb.m_closestHitFraction;
        out.body = world->GetHandleFromCollisionObject(cb.m_hitCollisionObject);
    }
    return out;
}

std::vector<OverlapResult> PhysicsQuery::OverlapTest(PhysicsWorld* world, CollisionShapeHandle shape,
                                                     const math::Vec3& position, const math::Quat& rotation) {
    std::vector<OverlapResult> results;
    if (!world) return results;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return results;

    btCollisionShape* s = CollisionShapeFactory::GetShape(shape);
    if (!s) return results;

    btCollisionObject obj;
    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    t.setRotation(ToBt(rotation));
    obj.setWorldTransform(t);
    obj.setCollisionShape(s);

    OverlapCallback cb;
    cb.world = world;
    cb.results = &results;
    w->contactTest(&obj, cb);
    return results;
}

std::vector<RigidBodyHandle> PhysicsQuery::QueryAABB(PhysicsWorld* world, const math::AABB& aabb) {
    std::vector<RigidBodyHandle> results;
    if (!world) return results;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return results;
    if (!aabb.IsValid()) return results;

    btBoxShape box(ToBt(aabb.GetExtents()));
    btCollisionObject obj;
    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(aabb.GetCenter()));
    obj.setWorldTransform(t);
    obj.setCollisionShape(&box);

    std::vector<OverlapResult> overlaps;
    OverlapCallback cb;
    cb.world = world;
    cb.results = &overlaps;
    w->contactTest(&obj, cb);

    for (const auto& r : overlaps) {
        if (r.body) results.push_back(r.body);
    }
    return results;
}

std::vector<RigidBodyHandle> PhysicsQuery::QuerySphere(PhysicsWorld* world, const math::Vec3& center, float radius) {
    std::vector<RigidBodyHandle> results;
    if (!world) return results;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (!w) return results;
    if (radius <= 0.0f) return results;

    btSphereShape sphere(radius);
    btCollisionObject obj;
    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(center));
    obj.setWorldTransform(t);
    obj.setCollisionShape(&sphere);

    std::vector<OverlapResult> overlaps;
    OverlapCallback cb;
    cb.world = world;
    cb.results = &overlaps;
    w->contactTest(&obj, cb);

    for (const auto& r : overlaps) {
        if (r.body) results.push_back(r.body);
    }
    return results;
}

} // namespace physics
} // namespace eyelib
