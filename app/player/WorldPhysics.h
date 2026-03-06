#pragma once

#include "PlayerConfig.h"
#include "api/physics/PhysicsWorld.h"
#include "api/physics/RigidBody.h"
#include "api/world/All.h"
#include "eyelib/math/Types.h"

#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace openeye::player {

class WorldPhysicsBridge {
public:
    ~WorldPhysicsBridge();

    WorldPhysicsBridge() = default;
    WorldPhysicsBridge(const WorldPhysicsBridge&) = delete;
    WorldPhysicsBridge& operator=(const WorldPhysicsBridge&) = delete;

    bool Initialize(eyelib::api::physics::PhysicsWorld::PhysicsBackendType backendType = eyelib::api::physics::PhysicsWorld::PhysicsBackendType::Jolt);
    void Shutdown();

    /**
     * Build physics bodies from world entities:
     * - Collider + RigidBody: full dynamics
     * - Collider only: static body (mass=0)
     * - RigidBody only: body with default 1x1x1 box shape
     */
    bool BuildFromWorld(eyelib::api::world::World& world);

    void StepPhysicsOnly(float deltaSeconds, int maxSubSteps, float fixedTimeStep);
    void SyncTransformsToWorld(eyelib::api::world::World& world);

    void Step(eyelib::api::world::World& world, float deltaSeconds,
              int maxSubSteps, float fixedTimeStep);

    bool GetVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out);
    bool SetVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value);
    bool GetAngularVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out);
    bool SetAngularVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value);

    /** Collisions from last physics step: entities that touched this frame. */
    std::vector<::eyelib::world::EntityId> GetCollisions(::eyelib::world::EntityId entity) const;

    /** Set physics body and world transform (teleport). */
    bool SetTransform(eyelib::api::world::World& world, ::eyelib::world::EntityId entity,
                      const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation);

private:
    struct BodyBinding {
        ::eyelib::world::EntityId entity;
        eyelib::physics::PhysicsBodyId body;
        eyelib::physics::PhysicsShapeId shape;
        bool syncTransform = true;
    };

    const eyelib::api::world::World::RigidBodyEntry* FindRigidBody(
        std::span<const eyelib::api::world::World::RigidBodyEntry> rigidBodies,
        ::eyelib::world::EntityId entity) const;
    const eyelib::api::world::World::ColliderEntry* FindCollider(
        std::span<const eyelib::api::world::World::ColliderEntry> colliders,
        ::eyelib::world::EntityId entity) const;
    const BodyBinding* FindBinding(::eyelib::world::EntityId entity) const;
    BodyBinding* FindBinding(::eyelib::world::EntityId entity);
    ::eyelib::world::EntityId EntityFromBody(eyelib::api::physics::RigidBodyHandle body) const;
    void InstallContactCallback();

    bool BuildBodyForEntity(eyelib::api::world::World& world,
                            const eyelib::api::world::World::ColliderEntry& collider,
                            const eyelib::api::world::World::RigidBodyEntry& rigidBody);
    bool BuildBodyForRigidBodyOnly(eyelib::api::world::World& world,
                                   const eyelib::api::world::World::RigidBodyEntry& rigidBody);

    std::unique_ptr<eyelib::api::physics::PhysicsWorld> m_physics;
    bool m_ready = false;
    std::vector<BodyBinding> m_bindings;
    /** Contact pairs from the last Step (entity A, entity B). Cleared at start of each step. */
    std::vector<std::pair<::eyelib::world::EntityId, ::eyelib::world::EntityId>> m_contactPairs;

    /** Fixed-timestep accumulator. Only advance physics for time elapsed while simulation was running. */
    float m_physicsAccumulator = 0.0f;
    /** First frame after init: do not add frame delta (it includes pre-simulation time). */
    bool m_physicsFirstFrame = true;
};

} // namespace openeye::player
