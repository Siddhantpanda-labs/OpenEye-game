#pragma once

#include "eyelib/render/RenderTypes.h"

namespace eyelib {
namespace ecs {
namespace components {

struct Camera {
    render::CameraView camera;
    bool isPrimary = false;

    bool isPerspective = true;
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float orthoSize = 10.0f;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
