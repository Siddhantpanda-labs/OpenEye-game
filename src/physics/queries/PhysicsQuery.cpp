#include "PhysicsQuery.h"
#include "PhysicsQueryTypes.h"
#include "../PhysicsWorld.h"
#include "../utils/PhysicsUtils.h"
#include <btBulletDynamicsCommon.h>

namespace OpenEye {
namespace Physics {

RaycastResult PhysicsQuery::Raycast(PhysicsWorld* world, const glm::vec3& from, const glm::vec3& to) {
    RaycastResult result;
    
    if (!world || !world->GetWorld()) return result;
    
    btVector3 btFrom = Utils::PhysicsUtils::GlmToBullet(from);
    btVector3 btTo = Utils::PhysicsUtils::GlmToBullet(to);
    
    btCollisionWorld::AllHitsRayResultCallback callback(btFrom, btTo);
    world->GetWorld()->rayTest(btFrom, btTo, callback);
    
    if (callback.hasHit()) {
        result.hit = true;
        // Get closest hit
        if (callback.m_hitPointWorld.size() > 0) {
            result.point = Utils::PhysicsUtils::BulletToGlm(callback.m_hitPointWorld[0]);
            result.normal = Utils::PhysicsUtils::BulletToGlm(callback.m_hitNormalWorld[0]);
            if (callback.m_collisionObjects.size() > 0) {
                result.userData = callback.m_collisionObjects[0]->getUserPointer();
            }
        }
    }
    
    return result;
}

RaycastResult PhysicsQuery::RaycastClosest(PhysicsWorld* world, const glm::vec3& from, const glm::vec3& to) {
    RaycastResult result;
    
    if (!world || !world->GetWorld()) return result;
    
    btVector3 btFrom = Utils::PhysicsUtils::GlmToBullet(from);
    btVector3 btTo = Utils::PhysicsUtils::GlmToBullet(to);
    
    btCollisionWorld::ClosestRayResultCallback callback(btFrom, btTo);
    world->GetWorld()->rayTest(btFrom, btTo, callback);
    
    if (callback.hasHit()) {
        result.hit = true;
        result.point = Utils::PhysicsUtils::BulletToGlm(callback.m_hitPointWorld);
        result.normal = Utils::PhysicsUtils::BulletToGlm(callback.m_hitNormalWorld);
        result.userData = callback.m_collisionObject->getUserPointer();
    }
    
    return result;
}

SweepResult PhysicsQuery::SweepTest(
    PhysicsWorld* world,
    std::shared_ptr<btCollisionShape> shape,
    const glm::vec3& from,
    const glm::vec3& to,
    const glm::quat& rotation
) {
    SweepResult result;
    
    if (!shape || !world || !world->GetWorld()) return result;
    
    // Sweep test requires convex shape
    btConvexShape* convexShape = dynamic_cast<btConvexShape*>(shape.get());
    if (!convexShape) {
        // Not a convex shape - cannot perform sweep test
        return result;
    }
    
    btTransform fromTransform;
    fromTransform.setIdentity();
    fromTransform.setOrigin(Utils::PhysicsUtils::GlmToBullet(from));
    fromTransform.setRotation(Utils::PhysicsUtils::GlmToBullet(rotation));
    
    btTransform toTransform;
    toTransform.setIdentity();
    toTransform.setOrigin(Utils::PhysicsUtils::GlmToBullet(to));
    toTransform.setRotation(Utils::PhysicsUtils::GlmToBullet(rotation));
    
    btCollisionWorld::ClosestConvexResultCallback callback(
        Utils::PhysicsUtils::GlmToBullet(from),
        Utils::PhysicsUtils::GlmToBullet(to)
    );
    
    world->GetWorld()->convexSweepTest(
        convexShape,
        fromTransform,
        toTransform,
        callback
    );
    
    if (callback.hasHit()) {
        result.hit = true;
        result.hitFraction = callback.m_closestHitFraction;
        result.hitPoint = Utils::PhysicsUtils::BulletToGlm(callback.m_hitPointWorld);
        result.hitNormal = Utils::PhysicsUtils::BulletToGlm(callback.m_hitNormalWorld);
        result.userData = callback.m_hitCollisionObject->getUserPointer();
    }
    
    return result;
}

} // namespace Physics
} // namespace OpenEye

