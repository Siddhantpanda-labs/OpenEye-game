#pragma once

#include "PlayerConfig.h"
#include "api/physics/All.h"
#include "api/world/All.h"

#include <span>
#include <vector>

namespace openeye::player {

class WorldPhysicsBridge {
public:
    bool Initialize();
    void Shutdown();

    bool BuildFromWorld(eyelib::api::world::World& world);
    void Step(eyelib::api::world::World& world, float deltaSeconds,
              int maxSubSteps, float fixedTimeStep);

    bool GetVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out);
    bool SetVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value);
    bool GetAngularVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out);
    bool SetAngularVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value);

private:
    struct BodyBinding {
        ::eyelib::world::EntityId entity;
        eyelib::api::physics::RigidBodyHandle body = 0;
        eyelib::api::physics::CollisionShapeHandle shape = 0;
        bool syncTransform = true;
    };

    const eyelib::api::world::World::RigidBodyEntry* FindRigidBody(
        std::span<const eyelib::api::world::World::RigidBodyEntry> rigidBodies,
        ::eyelib::world::EntityId entity) const;
    const BodyBinding* FindBinding(::eyelib::world::EntityId entity) const;
    BodyBinding* FindBinding(::eyelib::world::EntityId entity);

    bool BuildBodyForCollider(eyelib::api::world::World& world,
                              const eyelib::api::world::World::ColliderEntry& collider,
                              std::span<const eyelib::api::world::World::RigidBodyEntry> rigidBodies);

    eyelib::api::physics::PhysicsWorld m_physics;
    bool m_ready = false;
    std::vector<BodyBinding> m_bindings;
};

} // namespace openeye::player
