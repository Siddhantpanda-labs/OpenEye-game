#include "ShadowPass.h"
#include "../ShadowMap.h"
#include "../../ecs/Scene.h"
#include "../../ecs/Systems.h"

namespace OpenEye { namespace Rendering {

void ShadowPass::Execute(ECS::Scene& scene, ShadowMap& shadowMap, uint32_t cascadeIndex, const glm::vec3& lightDir) {
    ECS::RenderSystem::RenderShadowPass(scene, shadowMap, cascadeIndex, lightDir);
}

} }


