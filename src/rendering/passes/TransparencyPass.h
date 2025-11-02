#pragma once

#include "../../ecs/Scene.h"
#include "../Camera.h"
#include "../resources/Shader.h"
#include "../Frustum.h"

namespace OpenEye {
namespace Rendering {

class TransparencyPass {
public:
    // Render transparent objects (forward pass, back-to-front sorted)
    static void Execute(ECS::Scene& scene, const Camera& camera, const Frustum& frustum);
};

} // namespace Rendering
} // namespace OpenEye

