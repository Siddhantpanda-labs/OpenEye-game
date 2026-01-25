#pragma once

#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace api {

struct Drawable {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    math::Mat4 transform{1.0f};
};

struct CameraView {
    math::Mat4 view{1.0f};
    math::Mat4 projection{1.0f};
    math::Vec3 viewPos{0, 0, 0};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct LightParams {
    math::Vec3 direction{0.5f, 1.0f, 0.3f};
    math::Vec3 color{1.0f, 0.95f, 0.9f};
    float intensity = 3.0f;
    math::Vec3 ambient{0.08f};
    float exposure = 1.0f;
};

struct PointLight {
    math::Vec3 position{0, 0, 0};
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float radius = 10.0f;
};

struct SpotLight {
    math::Vec3 position{0, 0, 0};
    math::Vec3 direction{0, -1, 0};
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float range = 10.0f;
    float innerConeCos = 0.9f;
    float outerConeCos = 0.7f;
};

struct RenderScene {
    std::vector<Drawable> opaque;
    std::vector<Drawable> transparent;
    CameraView camera;
    LightParams light;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;
};

} // namespace api
} // namespace eyelib
