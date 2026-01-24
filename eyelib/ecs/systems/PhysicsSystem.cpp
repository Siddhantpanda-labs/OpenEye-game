#include "PhysicsSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/RigidBody.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace ecs {

void PhysicsSystem::Update(World& world, float /*deltaTime*/) {
    auto* physicsWorld = world.GetPhysicsWorld();
    if (!physicsWorld) return;

    auto view = world.Registry().view<components::Transform, components::RigidBody>();
    for (auto entity : view) {
        auto& transform = view.get<components::Transform>(entity);
        auto& rigidBody = view.get<components::RigidBody>(entity);
        if (rigidBody.bodyHandle == 0) continue;

        if (rigidBody.syncTransform) {
            if (auto t = physicsWorld->GetRigidBodyTransform(rigidBody.bodyHandle)) {
                transform.position = t->position;
                transform.rotation = t->rotation;
                transform.MarkDirty();
            }
        } else {
            physicsWorld->SetRigidBodyTransform(rigidBody.bodyHandle,
                transform.position, transform.rotation);
        }
    }
}

} // namespace ecs
} // namespace eyelib
