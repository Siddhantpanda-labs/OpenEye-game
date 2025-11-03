#include "RigidBodyFactory.h"
#include "../PhysicsWorld.h"
#include "../utils/PhysicsUtils.h"
#include "../../core/log/Log.h"

namespace OpenEye {
namespace Physics {

btRigidBody* RigidBodyFactory::Create(
    PhysicsWorld* world,
    const glm::vec3& position,
    const glm::quat& rotation,
    std::shared_ptr<btCollisionShape> shape,
    float mass,
    const glm::vec3& inertia
) {
    if (!shape) {
        OE_WARN("RigidBodyFactory::Create: Invalid shape provided");
        return nullptr;
    }
    
    if (!world || !world->GetWorld()) {
        OE_WARN("RigidBodyFactory::Create: PhysicsWorld not initialized!");
        return nullptr;
    }
    
    // Calculate local inertia
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    if (mass > 0.0f) {
        if (inertia.x > 0.0f || inertia.y > 0.0f || inertia.z > 0.0f) {
            localInertia = Utils::PhysicsUtils::GlmToBullet(inertia);
        } else {
            shape->calculateLocalInertia(mass, localInertia);
        }
    }
    
    // Create motion state (handles sync with Transform component)
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(Utils::PhysicsUtils::GlmToBullet(position));
    startTransform.setRotation(Utils::PhysicsUtils::GlmToBullet(rotation));
    
    auto motionState = new btDefaultMotionState(startTransform);
    
    // Create rigid body construction info
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape.get(), localInertia);
    btRigidBody* body = new btRigidBody(rbInfo);
    
    // Add to world
    world->GetWorld()->addRigidBody(body);
    
    return body;
}

void RigidBodyFactory::Destroy(PhysicsWorld* world, btRigidBody* body) {
    if (!body || !world || !world->GetWorld()) return;
    
    // Remove from world
    world->GetWorld()->removeRigidBody(body);
    
    // Clean up motion state
    if (body->getMotionState()) {
        delete body->getMotionState();
    }
    
    // Delete body (shape is managed by shared_ptr, so it's fine)
    delete body;
}

bool RigidBodyFactory::ExistsInWorld(PhysicsWorld* world, btRigidBody* body) {
    if (!world || !world->GetWorld() || !body) return false;
    
    btDynamicsWorld* btWorld = world->GetWorld();
    for (int i = 0; i < btWorld->getNumCollisionObjects(); ++i) {
        btCollisionObject* obj = btWorld->getCollisionObjectArray()[i];
        if (obj == body) return true;
    }
    
    return false;
}

} // namespace Physics
} // namespace OpenEye

