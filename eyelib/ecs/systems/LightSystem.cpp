#include "LightSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Light.h"

namespace eyelib {
namespace ecs {

void LightSystem::Update(World& /*world*/, float /*deltaTime*/) {
    // Light components are read by RenderSystem; no per-frame mutation needed here.
}

} // namespace ecs
} // namespace eyelib
