#include "LightingPass.h"
#include "../GBuffer.h"
#include "../Camera.h"
#include "../ShadowMap.h"
#include "../SkyboxRenderer.h"
#include "../IBL.h"
#include "../RenderPipeline.h"
#include "../../ecs/Scene.h"
#include "../../ecs/Systems.h"

namespace OpenEye { namespace Rendering {

void LightingPass::Execute(ECS::Scene& scene,
                           GBuffer& gbuffer,
                           const Camera& camera,
                           std::shared_ptr<Shader> lightingShader,
                           ShadowMap* shadowMap,
                           const LightingParams& params,
                           SkyboxRenderer* skyboxRenderer,
                           IBL* ibl) {
    ECS::RenderSystem::RenderLightingPass(scene, gbuffer, camera, lightingShader, shadowMap,
        params.debugMode, params.lightDir, params.lightColor, params.lightIntensity,
        params.ambient, params.exposure, skyboxRenderer, ibl);
}

} }


