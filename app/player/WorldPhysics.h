#pragma once

#include "PlayerConfig.h"
#include "api/physics/PhysicsWorld.h"
#include "api/physics/RigidBody.h"
#include "api/world/All.h"

#include <memory>
#include <span>
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
     * Build physics bodies from world entities that have BOTH Collider AND RigidBody.
     * Entities with only a Collider (no RigidBody) are skipped — Collider is a shape
     * description, RigidBody is the dynamics component that authorizes body creation.
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
    const BodyBinding* FindBinding(::eyelib::world::EntityId entity) const;
    BodyBinding* FindBinding(::eyelib::world::EntityId entity);

    bool BuildBodyForEntity(eyelib::api::world::World& world,
                            const eyelib::api::world::World::ColliderEntry& collider,
                            const eyelib::api::world::World::RigidBodyEntry& rigidBody);

    std::unique_ptr<eyelib::api::physics::PhysicsWorld> m_physics;
    bool m_ready = false;
    std::vector<BodyBinding> m_bindings;
};

} // namespace openeye::player
