#pragma once

namespace eyelib {
namespace world {
namespace components {

struct Camera {
    bool isPrimary = false;
    bool isPerspective = true;
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float orthoSize = 10.0f;
};

} // namespace components
} // namespace world
} // namespace eyelib
