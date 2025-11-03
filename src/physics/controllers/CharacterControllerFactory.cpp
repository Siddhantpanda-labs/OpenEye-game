#include "CharacterControllerFactory.h"
#include "../PhysicsWorld.h"
#include "../utils/PhysicsUtils.h"
#include "../../core/log/Log.h"
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

namespace OpenEye {
namespace Physics {

btKinematicCharacterController* CharacterControllerFactory::Create(
    PhysicsWorld* world,
    const glm::vec3& position,
    float radius,
    float height
) {
    if (!world || !world->GetWorld()) {
        OE_WARN("CharacterControllerFactory::Create: PhysicsWorld not initialized!");
        return nullptr;
    }
    
    // Create ghost object (collision detection without physics response)
    auto ghostObject = new btPairCachingGhostObject();
    
    // Create capsule shape for character
    float totalHeight = height + radius * 2.0f;
    auto shape = std::make_shared<btCapsuleShape>(radius, totalHeight);
    ghostObject->setCollisionShape(shape.get());
    
    // Set position
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(Utils::PhysicsUtils::GlmToBullet(position));
    ghostObject->setWorldTransform(transform);
    
    // Set collision flags
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    
    // Create character controller
    float stepHeight = radius * 0.4f;  // 40% of radius
    auto controller = new btKinematicCharacterController(
        ghostObject,
        shape.get(),
        stepHeight
    );
    
    // Add ghost object to world
    world->GetWorld()->addCollisionObject(
        ghostObject, 
        btBroadphaseProxy::CharacterFilter, 
        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter
    );
    
    // Store shape to keep it alive (controller holds reference)
    
    return controller;
}

void CharacterControllerFactory::Destroy(PhysicsWorld* world, btKinematicCharacterController* controller) {
    if (!controller || !world || !world->GetWorld()) return;
    
    // Get ghost object
    btPairCachingGhostObject* ghostObject = static_cast<btPairCachingGhostObject*>(controller->getGhostObject());
    
    // Remove from world
    world->GetWorld()->removeCollisionObject(ghostObject);
    
    // Clean up
    delete controller;
    delete ghostObject;
}

bool CharacterControllerFactory::ExistsInWorld(PhysicsWorld* world, btKinematicCharacterController* controller) {
    if (!world || !world->GetWorld() || !controller) return false;
    
    btPairCachingGhostObject* ghost = static_cast<btPairCachingGhostObject*>(controller->getGhostObject());
    if (!ghost) return false;
    
    btDynamicsWorld* btWorld = world->GetWorld();
    for (int i = 0; i < btWorld->getNumCollisionObjects(); ++i) {
        btCollisionObject* obj = btWorld->getCollisionObjectArray()[i];
        if (obj == ghost) return true;
    }
    
    return false;
}

} // namespace Physics
} // namespace OpenEye

