#include "PhysicsSystem.h"

#include "eyelib/ecs/World.h"

namespace eyelib {
namespace ecs {

void PhysicsSystem::Update(World& /*world*/, float /*deltaTime*/) {
    // Physics transform sync is handled by WorldPhysicsBridge (app layer).
    // The ECS PhysicsSystem no longer accesses body handles directly.
}

} // namespace ecs
} // namespace eyelib
