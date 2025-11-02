#pragma once

namespace OpenEye { namespace ECS { class Scene; } }
namespace OpenEye { namespace Rendering { class Camera; class GBuffer; class Frustum; } }
#include <glm/glm.hpp>

namespace OpenEye { namespace Rendering {

class GBufferPass {
public:
    void Execute(ECS::Scene& scene,
                 GBuffer& gbuffer,
                 const Camera& camera,
                 const glm::mat4& view,
                 const glm::mat4& projection,
                 const Frustum& frustum);
};

} }


