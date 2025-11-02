#include "GBufferPass.h"
#include "../GBuffer.h"
#include "../Camera.h"
#include "../Frustum.h"
#include "../../ecs/Scene.h"
#include "../../ecs/Systems.h"

namespace OpenEye { namespace Rendering {

void GBufferPass::Execute(ECS::Scene& scene,
                          GBuffer& gbuffer,
                          const Camera& camera,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          const Frustum& frustum,
                          bool enableCulling,
                          bool enableLOD,
                          bool enableInstancing,
                          size_t minInstancesForBatch) {
    ECS::RenderSystem::RenderGeometryPass(scene, gbuffer, camera, view, projection, frustum,
        enableCulling, enableLOD, enableInstancing, minInstancesForBatch);
}

} }


