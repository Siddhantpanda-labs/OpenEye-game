#include "TransformSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Transform.h"

namespace eyelib {
namespace ecs {

void TransformSystem::Update(World& world, float /*deltaTime*/) {
    auto view = world.Registry().view<components::Transform>();
    for (auto entity : view) {
        auto& transform = view.get<components::Transform>(entity);
        transform.GetWorldMatrix(world);
    }
}

} // namespace ecs
} // namespace eyelib
