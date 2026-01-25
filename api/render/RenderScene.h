#pragma once

#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "api/render/Light.h"
#include "api/render/TextureCube.h"
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

using DirectionalLight = ::eyelib::api::render::DirectionalLight;
using PointLight = ::eyelib::api::render::PointLight;
using SpotLight = ::eyelib::api::render::SpotLight;

struct RenderScene {
    std::vector<Drawable> opaque;
    std::vector<Drawable> transparent;
    CameraView camera;
    DirectionalLight light;
    std::shared_ptr<TextureCube> skybox;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;
};

} // namespace api
} // namespace eyelib
