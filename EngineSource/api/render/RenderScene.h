#pragma once

#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "api/render/MaterialInstance.h"
#include "api/render/Light.h"
#include "api/render/Texture.h"
#include "api/render/TextureCube.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace eyelib {
namespace api {

/**
 * Immutable per-drawable render parameters (Phase 4 snapshot).
 * Filled on the game thread at scene build time; render thread reads only this.
 */
struct DrawableRenderParams {
    math::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    math::Vec3 emission{0.0f, 0.0f, 0.0f};
    float alpha = 1.0f;
    math::Vec4 uvTransform{0.0f, 0.0f, 1.0f, 1.0f};
    float uvRotation = 0.0f;
    float texCoordIndex = 0.0f;
    float flipV = 0.0f;
    std::shared_ptr<Texture> albedoTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> metallicRoughnessTexture;
    std::shared_ptr<Texture> aoTexture;
    std::shared_ptr<Texture> emissiveTexture;
    std::shared_ptr<Texture> metallicTexture;
    std::shared_ptr<Texture> roughnessTexture;
};

/** Fills \a out from \a material and optional \a materialInstance (game thread only). */
void FillDrawableRenderParams(DrawableRenderParams& out,
                              const std::shared_ptr<Material>& material,
                              const std::shared_ptr<MaterialInstance>& materialInstance);

struct Drawable {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::shared_ptr<MaterialInstance> materialInstance;
    DrawableRenderParams params;
    math::Mat4 transform{1.0f};
    const math::Mat4* bones = nullptr;
    size_t boneCount = 0;
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

/**
 * POD-like scene: raw arrays + counts. All array memory is owned by a FrameAllocator
 * (filled in BuildScene when frameAlloc is provided) or by optional internal storage
 * (when frameAlloc is null, e.g. single-threaded path). Reset only on render thread in ReleaseFrame.
 */
struct RenderScene {
    Drawable* opaque = nullptr;
    size_t opaqueCount = 0;
    Drawable* transparent = nullptr;
    size_t transparentCount = 0;
    PointLight* pointLights = nullptr;
    size_t pointLightCount = 0;
    SpotLight* spotLights = nullptr;
    size_t spotLightCount = 0;

    CameraView camera;
    DirectionalLight light;
    std::shared_ptr<TextureCube> skybox;

    /** Used only when building without a frame allocator (e.g. WorldRender::Render single-threaded). */
    std::vector<Drawable> opaqueStorage;
    std::vector<Drawable> transparentStorage;
    std::vector<PointLight> pointLightsStorage;
    std::vector<SpotLight> spotLightsStorage;
};

} // namespace api
} // namespace eyelib
