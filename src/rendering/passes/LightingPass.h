#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace OpenEye { namespace ECS { class Scene; } }
namespace OpenEye { namespace Rendering { class GBuffer; class Camera; class Shader; class ShadowMap; class SkyboxRenderer; } }

namespace OpenEye { namespace Rendering {

struct LightingParams;

class LightingPass {
public:
    void Execute(ECS::Scene& scene,
                 GBuffer& gbuffer,
                 const Camera& camera,
                 std::shared_ptr<Shader> lightingShader,
                 ShadowMap* shadowMap,
                 const LightingParams& params,
                 SkyboxRenderer* skyboxRenderer);
};

} }


