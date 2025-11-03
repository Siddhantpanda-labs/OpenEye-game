#pragma once

#include "../Scene.h"
#include "rendering/Camera.h"

namespace OpenEye {
namespace ECS {

// Camera system - manages camera components and finds primary camera
class CameraSystem {
public:
    static entt::entity FindPrimaryCamera(entt::registry& registry);
    static Rendering::Camera* GetPrimaryCamera(entt::registry& registry);
};

} // namespace ECS
} // namespace OpenEye

