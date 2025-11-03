#include "PhysicsWorld.h"
#include "shapes/CollisionShapeFactory.h"
#include "bodies/RigidBodyFactory.h"
#include "controllers/CharacterControllerFactory.h"
#include "queries/PhysicsQuery.h"
#include "utils/PhysicsUtils.h"
#include "../core/log/Log.h"
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

namespace OpenEye {
namespace Physics {

PhysicsWorld::PhysicsWorld() {
    // Initialize pointers to nullptr
}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
}

void PhysicsWorld::Initialize(const glm::vec3& gravity) {
    if (m_world) {
        OE_WARN("PhysicsWorld already initialized!");
        return;
    }
    
    // Setup collision configuration
    m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    
    // Setup dispatcher
    m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
    
    // Setup broadphase (Sweep and Prune - good for dynamic objects)
    m_broadphase = std::make_unique<btDbvtBroadphase>();
    
    // Setup constraint solver
    m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    
    // Create dynamics world
    m_world = std::make_unique<btDiscreteDynamicsWorld>(
        m_dispatcher.get(),
        m_broadphase.get(),
        m_solver.get(),
        m_collisionConfig.get()
    );
    
    // Set gravity
    SetGravity(gravity);
    
    OE_INFO("PhysicsWorld initialized successfully");
}

void PhysicsWorld::Shutdown() {
    if (!m_world) return;
    
    // Remove all objects from world
    for (int i = m_world->getNumCollisionObjects() - 1; i >= 0; --i) {
        btCollisionObject* obj = m_world->getCollisionObjectArray()[i];
        btRigidBody* body = btRigidBody::upcast(obj);
        if (body && body->getMotionState()) {
            delete body->getMotionState();
        }
        m_world->removeCollisionObject(obj);
        delete obj;
    }
    
    // Clear ghost objects (character controllers)
    for (int i = m_world->getNumCollisionObjects() - 1; i >= 0; --i) {
        btCollisionObject* obj = m_world->getCollisionObjectArray()[i];
        if (obj->getInternalType() == btCollisionObject::CO_GHOST_OBJECT) {
            m_world->removeCollisionObject(obj);
            delete obj;
        }
    }
    
    // Destroy in reverse order of creation
    m_world.reset();
    m_solver.reset();
    m_broadphase.reset();
    m_dispatcher.reset();
    m_collisionConfig.reset();
    
    OE_INFO("PhysicsWorld shut down");
}

void PhysicsWorld::Step(float deltaTime, int maxSubSteps) {
    if (!m_world) {
        OE_WARN("PhysicsWorld::Step() called before Initialize()!");
        return;
    }
    
    OnPreStep();
    m_world->stepSimulation(deltaTime, maxSubSteps);
    OnPostStep();
}

// ============================================================================
// Collision Shape Factory Delegates
// ============================================================================

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateBoxShape(const glm::vec3& halfExtents) {
    return CollisionShapeFactory::CreateBox(halfExtents);
}

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateSphereShape(float radius) {
    return CollisionShapeFactory::CreateSphere(radius);
}

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateCapsuleShape(float radius, float height) {
    return CollisionShapeFactory::CreateCapsule(radius, height);
}

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateCylinderShape(const glm::vec3& halfExtents) {
    return CollisionShapeFactory::CreateCylinder(halfExtents);
}

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateConvexHullShape(std::shared_ptr<Rendering::Mesh> mesh) {
    return CollisionShapeFactory::CreateConvexHull(mesh);
}

std::shared_ptr<btCollisionShape> PhysicsWorld::CreateBvhTriangleMeshShape(std::shared_ptr<Rendering::Mesh> mesh) {
    return CollisionShapeFactory::CreateBvhTriangleMesh(mesh);
}

// ============================================================================
// Rigid Body Factory Delegates
// ============================================================================

btRigidBody* PhysicsWorld::CreateRigidBody(
    const glm::vec3& position,
    const glm::quat& rotation,
    std::shared_ptr<btCollisionShape> shape,
    float mass,
    const glm::vec3& inertia
) {
    return RigidBodyFactory::Create(this, position, rotation, shape, mass, inertia);
}

void PhysicsWorld::DestroyRigidBody(btRigidBody* body) {
    RigidBodyFactory::Destroy(this, body);
}

bool PhysicsWorld::HasRigidBody(btRigidBody* body) const {
    return RigidBodyFactory::ExistsInWorld(const_cast<PhysicsWorld*>(this), body);
}

// ============================================================================
// Character Controller Factory Delegates
// ============================================================================

btKinematicCharacterController* PhysicsWorld::CreateCharacterController(
    const glm::vec3& position,
    float radius,
    float height
) {
    return CharacterControllerFactory::Create(this, position, radius, height);
}

void PhysicsWorld::DestroyCharacterController(btKinematicCharacterController* controller) {
    CharacterControllerFactory::Destroy(this, controller);
}

bool PhysicsWorld::HasCharacterController(btKinematicCharacterController* controller) const {
    return CharacterControllerFactory::ExistsInWorld(const_cast<PhysicsWorld*>(this), controller);
}

// ============================================================================
// Query Delegates
// ============================================================================

RaycastResult PhysicsWorld::Raycast(const glm::vec3& from, const glm::vec3& to) {
    return PhysicsQuery::Raycast(this, from, to);
}

RaycastResult PhysicsWorld::RaycastClosest(const glm::vec3& from, const glm::vec3& to) {
    return PhysicsQuery::RaycastClosest(this, from, to);
}

SweepResult PhysicsWorld::SweepTest(
    std::shared_ptr<btCollisionShape> shape,
    const glm::vec3& from,
    const glm::vec3& to,
    const glm::quat& rotation
) {
    return PhysicsQuery::SweepTest(this, shape, from, to, rotation);
}

// ============================================================================
// Settings
// ============================================================================

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
    if (m_world) {
        m_world->setGravity(Utils::PhysicsUtils::GlmToBullet(gravity));
    }
}

glm::vec3 PhysicsWorld::GetGravity() const {
    if (m_world) {
        return Utils::PhysicsUtils::BulletToGlm(m_world->getGravity());
    }
    return glm::vec3(0.0f);
}

void PhysicsWorld::OnPreStep() {
    // Hook for pre-step callbacks (e.g., sync Transform -> PhysicsBody)
}

void PhysicsWorld::OnPostStep() {
    // Hook for post-step callbacks (e.g., sync PhysicsBody -> Transform)
}

} // namespace Physics
} // namespace OpenEye
