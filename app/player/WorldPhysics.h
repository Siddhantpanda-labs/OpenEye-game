#pragma once

#include "PlayerConfig.h"
#include "api/physics/All.h"
#include "api/world/All.h"

#include <vector>

namespace openeye::player {

class WorldPhysicsBridge {
public:
    bool Initialize();
    void Shutdown();

    bool BuildFromWorld(eyelib::api::world::World& world);
    void Step(eyelib::api::world::World& world, float deltaSeconds,
              int maxSubSteps, float fixedTimeStep);

private:
    struct BodyBinding {
        ::eyelib::world::EntityId entity;
        eyelib::api::physics::RigidBodyHandle body = 0;
        eyelib::api::physics::CollisionShapeHandle shape = 0;
        bool syncTransform = true;
    };

    const eyelib::world::components::Transform* FindTransform(
        const std::vector<eyelib::api::world::World::TransformEntry>& transforms,
        ::eyelib::world::EntityId entity) const;

    const eyelib::api::world::World::RigidBodyEntry* FindRigidBody(
        const std::vector<eyelib::api::world::World::RigidBodyEntry>& rigidBodies,
        ::eyelib::world::EntityId entity) const;

    bool BuildBodyForCollider(eyelib::api::world::World& world,
                              const eyelib::api::world::World::ColliderEntry& collider,
                              const std::vector<eyelib::api::world::World::TransformEntry>& transforms,
                              const std::vector<eyelib::api::world::World::RigidBodyEntry>& rigidBodies);

    eyelib::api::physics::PhysicsWorld m_physics;
    bool m_ready = false;
    std::vector<BodyBinding> m_bindings;
};

} // namespace openeye::player
