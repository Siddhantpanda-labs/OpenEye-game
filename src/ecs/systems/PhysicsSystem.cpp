#include "PhysicsSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "../../physics/PhysicsWorld.h"
#include "../../physics/utils/PhysicsUtils.h"
#include "../../rendering/resources/MeshLoader.h"
#include "../../core/log/Log.h"
#include <btBulletDynamicsCommon.h>
#include <glm/gtc/matrix_transform.hpp>

namespace OpenEye {
namespace ECS {

void PhysicsSystem::Update(Scene& scene, Physics::PhysicsWorld* physicsWorld, float deltaTime) {
    if (!physicsWorld) {
        Core::Log::Warn("PhysicsSystem::Update: PhysicsWorld is null!");
        return;
    }
    
    // 1. Sync Transform -> PhysicsBody (before step)
    SyncTransformToPhysics(scene);
    
    // 2. Step physics simulation
    physicsWorld->Step(deltaTime);
    
    // 3. Sync PhysicsBody -> Transform (after step)
    SyncPhysicsToTransform(scene);
}

void PhysicsSystem::SyncTransformToPhysics(Scene& scene) {
    auto& registry = scene.GetRegistry();
    
    // Get all entities with both Transform and PhysicsBody
    auto view = registry.view<Transform, PhysicsBody>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& physicsBody = view.get<PhysicsBody>(entity);
        
        // Skip if sync disabled or no rigid body
        if (!physicsBody.syncTransformToPhysics || !physicsBody.rigidBody) {
            continue;
        }
        
        // Get world transform (handles hierarchy)
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        
        // Extract position and rotation from world matrix
        glm::vec3 position = glm::vec3(worldMatrix[3]);
        glm::quat rotation = glm::quat_cast(worldMatrix);
        
        // Update Bullet transform
        btTransform btTransform;
        btTransform.setOrigin(Physics::Utils::PhysicsUtils::GlmToBullet(position));
        btTransform.setRotation(Physics::Utils::PhysicsUtils::GlmToBullet(rotation));
        
        // For static bodies (mass = 0), always sync Transform -> Physics
        // For kinematic bodies, always sync Transform -> Physics (they're manually controlled)
        // For dynamic bodies, we only sync on initial setup or when transform is manually changed
        // (Dynamic bodies are controlled by physics, so we don't sync every frame to avoid teleporting)
        if (physicsBody.mass == 0.0f || physicsBody.isKinematic) {
            // Static or kinematic: always sync Transform -> Physics
            physicsBody.rigidBody->setWorldTransform(btTransform);
        }
        // Dynamic bodies: Don't sync Transform -> Physics every frame
        // (They're controlled by physics simulation)
        // Initial positioning is handled in AttachPhysicsBody
    }
}

void PhysicsSystem::SyncPhysicsToTransform(Scene& scene) {
    auto& registry = scene.GetRegistry();
    
    // Get all entities with both Transform and PhysicsBody
    auto view = registry.view<Transform, PhysicsBody>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& physicsBody = view.get<PhysicsBody>(entity);
        
        // Skip if sync disabled or no rigid body
        if (!physicsBody.syncPhysicsToTransform || !physicsBody.rigidBody) {
            continue;
        }
        
        // Skip static bodies (mass = 0) - they don't move via physics
        if (physicsBody.mass == 0.0f) {
            continue;
        }
        
        // Skip kinematic objects (they don't move via physics)
        if (physicsBody.isKinematic) {
            continue;
        }
        
        // Get transform from Bullet
        btTransform btTransform;
        if (physicsBody.mass > 0.0f) {
            // Dynamic body - get from motion state
            physicsBody.rigidBody->getMotionState()->getWorldTransform(btTransform);
        } else {
            // Static body - get from rigid body directly
            btTransform = physicsBody.rigidBody->getWorldTransform();
        }
        
        // Convert to GLM
        glm::vec3 position = Physics::Utils::PhysicsUtils::BulletToGlm(btTransform.getOrigin());
        glm::quat rotation = Physics::Utils::PhysicsUtils::BulletToGlm(btTransform.getRotation());
        
        // Update Transform component (only if entity has no parent, or update local transform)
        if (transform.parent == entt::null) {
            // Root entity - update directly
            transform.position = position;
            transform.rotation = rotation;
            transform.dirty = true;  // Mark dirty so world matrix recalculates
        } else {
            // Child entity - need to compute local transform from parent
            // For now, we'll update position directly (this is a simplification)
            // In a full implementation, you'd compute local transform from parent's world transform
            transform.position = position;
            transform.rotation = rotation;
            transform.dirty = true;
        }
        
        // Debug: Log first few physics updates for ball to verify it's moving
        static int ballUpdateCount = 0;
        static float lastBallY = position.y;
        if (ballUpdateCount < 10) {
            Core::Log::Info("PhysicsSystem: Ball position from physics: (" + 
                std::to_string(position.x) + ", " + 
                std::to_string(position.y) + ", " + 
                std::to_string(position.z) + 
                ") deltaY=" + std::to_string(position.y - lastBallY));
            lastBallY = position.y;
            ballUpdateCount++;
        }
    }
}

bool PhysicsSystem::AttachPhysicsBody(
    Scene& scene,
    Entity entity,
    Physics::PhysicsWorld* physicsWorld,
    std::shared_ptr<btCollisionShape> shape,
    float mass,
    bool isKinematic
) {
    if (!physicsWorld || !shape) {
        Core::Log::Warn("PhysicsSystem::AttachPhysicsBody: Invalid parameters");
        return false;
    }
    
    if (!scene.GetRegistry().valid(entity)) {
        Core::Log::Warn("PhysicsSystem::AttachPhysicsBody: Invalid entity");
        return false;
    }
    
    // Ensure entity has Transform component
    if (!scene.GetRegistry().all_of<Transform>(entity)) {
        Core::Log::Warn("PhysicsSystem::AttachPhysicsBody: Entity must have Transform component");
        return false;
    }
    
    auto& transform = scene.GetRegistry().get<Transform>(entity);
    
    // Get position and rotation from transform
    glm::vec3 position = transform.position;
    glm::quat rotation = transform.rotation;
    
    // Create rigid body
    btRigidBody* rigidBody = physicsWorld->CreateRigidBody(
        position,
        rotation,
        shape,
        mass,
        glm::vec3(0.0f)  // Auto-calculate inertia
    );
    
    if (!rigidBody) {
        Core::Log::Error("PhysicsSystem::AttachPhysicsBody: Failed to create rigid body");
        return false;
    }
    
    // Set kinematic flag if needed
    if (isKinematic || mass == 0.0f) {
        rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | 
                                    (mass == 0.0f ? btCollisionObject::CF_STATIC_OBJECT : 
                                     btCollisionObject::CF_KINEMATIC_OBJECT));
        if (mass == 0.0f) {
            rigidBody->setActivationState(DISABLE_SIMULATION);
        }
    }
    
    // Create PhysicsBody component
    PhysicsBody physicsBody;
    physicsBody.rigidBody = rigidBody;
    physicsBody.collisionShape = shape;
    physicsBody.mass = mass;
    physicsBody.isKinematic = isKinematic;
    
    // Attach to entity
    scene.GetRegistry().emplace<PhysicsBody>(entity, physicsBody);
    
    Core::Log::Info("PhysicsSystem: Attached physics body to entity (mass=" + std::to_string(mass) + 
            ", isKinematic=" + (isKinematic ? "true" : "false") + ")");
    
    return true;
}

void PhysicsSystem::DetachPhysicsBody(Scene& scene, Entity entity, Physics::PhysicsWorld* physicsWorld) {
    if (!physicsWorld || !scene.GetRegistry().valid(entity)) {
        return;
    }
    
    if (!scene.GetRegistry().all_of<PhysicsBody>(entity)) {
        return;
    }
    
    auto& physicsBody = scene.GetRegistry().get<PhysicsBody>(entity);
    
    if (physicsBody.rigidBody) {
        physicsWorld->DestroyRigidBody(physicsBody.rigidBody);
        physicsBody.rigidBody = nullptr;
    }
    
    scene.GetRegistry().remove<PhysicsBody>(entity);
}

Entity PhysicsSystem::CreateGroundPlane(
    Scene& scene,
    Physics::PhysicsWorld* physicsWorld,
    const glm::vec3& position,
    const glm::vec2& size
) {
    // Create entity
    Entity groundEntity = scene.CreateEntity("GroundPlane");
    
    // Set transform
    auto& transform = scene.GetRegistry().get<Transform>(groundEntity);
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity rotation
    
    // Create collision shape (box shape for ground)
    // Make ground thin (height = 0.2 units) so objects sit on top surface
    glm::vec3 halfExtents(size.x * 0.5f, 0.1f, size.y * 0.5f); // Height = 0.2 units total
    auto groundShape = physicsWorld->CreateBoxShape(halfExtents);
    
    // Position ground so top surface is at position.y
    // Physics center should be at position.y - halfExtents.y
    glm::vec3 physicsCenter = position;
    physicsCenter.y = position.y - halfExtents.y; // Shift down by half height
    
    // Visual: cube mesh is 1x1x1, scale to match physics size
    // Visual height = 0.2, so scale.y = 0.2
    transform.scale = glm::vec3(size.x, 0.2f, size.y);
    // Visual position: top surface at position.y, so center is at position.y - 0.1
    transform.position = physicsCenter;
    
    // Create visual representation (simple box mesh)
    auto groundMesh = Rendering::MeshLoader::CreateCube();
    
    // Create material
    auto groundShader = std::make_shared<Rendering::Shader>();
    groundShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", 
                               "assets/shaders/passes/deferred/gbuffer.frag");
    auto groundMaterial = std::make_shared<Rendering::Material>(groundShader);
    groundMaterial->SetAlbedo(glm::vec3(0.5f, 0.5f, 0.5f));  // Gray
    groundMaterial->SetMetallic(0.0f);
    groundMaterial->SetRoughness(0.8f);
    
    scene.GetRegistry().emplace<Renderable>(groundEntity, groundMesh, groundMaterial);
    scene.GetRegistry().emplace<AABB>(groundEntity);
    
    // Attach physics body (static, mass = 0) at physics center
    if (AttachPhysicsBody(scene, groundEntity, physicsWorld, groundShape, 0.0f, false)) {
        // Set ground properties (high friction, no bounce for ball to bounce properly)
        auto& physicsBody = scene.GetRegistry().get<PhysicsBody>(groundEntity);
        if (physicsBody.rigidBody) {
            physicsBody.rigidBody->setRestitution(0.0f); // Non-bouncy ground (ball bounces due to its own restitution)
            physicsBody.rigidBody->setFriction(0.8f);     // High friction for realism
        }
    }
    
    Core::Log::Info("PhysicsSystem: Created ground plane at (" + 
            std::to_string(position.x) + ", " + 
            std::to_string(position.y) + ", " + 
            std::to_string(position.z) + ") with size (" + 
            std::to_string(size.x) + ", " + std::to_string(size.y) + ")");
    
    return groundEntity;
}

} // namespace ECS
} // namespace OpenEye

