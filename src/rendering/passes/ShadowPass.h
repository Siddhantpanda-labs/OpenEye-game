#pragma once

#include <glm/glm.hpp>

namespace OpenEye { namespace ECS { class Scene; } }
namespace OpenEye { namespace Rendering { class ShadowMap; } }

namespace OpenEye { namespace Rendering {

class ShadowPass {
public:
    void Execute(ECS::Scene& scene, ShadowMap& shadowMap, uint32_t cascadeIndex, const glm::vec3& lightDir);
};

} }


