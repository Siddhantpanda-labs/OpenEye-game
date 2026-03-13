#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Mesh;
class Material;
class MaterialInstance;
class TextureCube;

/** Single drawable: mesh + material + world transform */
struct Drawable {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::shared_ptr<MaterialInstance> materialInstance;
    math::Mat4 transform{1.0f};
    const math::Mat4* bones = nullptr;
    size_t boneCount = 0;
    /** Submesh index (0 = first). -1 = draw all. */
    int32_t submeshIndex = -1;
};

/** Camera view: view/projection matrices and view position */
struct CameraView {
    math::Mat4 view{1.0f};
    math::Mat4 projection{1.0f};
    math::Vec3 viewPos{0, 0, 0};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

/** Directional light parameters */
struct LightParams {
    math::Vec3 direction{0.5f, 1.0f, 0.3f};
    math::Vec3 color{1.0f, 0.95f, 0.9f};
    float intensity = 3.0f;
    math::Vec3 ambient{0.08f};
    float exposure = 1.0f;
    bool castShadows = true;
};

/** Point light (for Forward+) — layout matches GLSL std140 for SSBO binding 3 */
#pragma pack(push, 4)
struct PointLight {
    math::Vec3 position{0, 0, 0};
    float _pad0 = 0.0f;
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float radius = 10.0f;
    float _pad1 = 0.0f, _pad2 = 0.0f, _pad3 = 0.0f;
};

/** Spot light (for Forward+) — layout matches GLSL std140 for SSBO binding 4 (stride 80) */
struct SpotLight {
    math::Vec3 position{0, 0, 0};
    float _pad0 = 0.0f;
    math::Vec3 direction{0, -1, 0};
    float _pad1 = 0.0f;
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float range = 10.0f;
    float innerConeCos = 0.9f;  // cos(inner angle)
    float outerConeCos = 0.7f;  // cos(outer angle)
    float _pad2 = 0.0f, _pad3 = 0.0f;
    float _pad4 = 0.0f, _pad5 = 0.0f, _pad6 = 0.0f;  // std140 struct size rounded to 80
};
#pragma pack(pop)

/** Minimal scene representation for rendering (no ECS) */
struct RenderScene {
    std::vector<Drawable> opaque;
    std::vector<Drawable> transparent;  // Sorted back-to-front by caller or pass
    CameraView camera;
    LightParams light;  // Directional light (always present)
    std::shared_ptr<TextureCube> skybox;
    
    // For Forward+ rendering
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;
};

/**
 * Immutable per-frame snapshot for the render pipeline (Phase B).
 * Filled once per frame by PrepareFrameData from IRenderSceneView + culling.
 * Passes and submission use this instead of re-querying the view.
 */
struct RenderFrameData {
    CameraView camera;
    LightParams light;
    std::shared_ptr<TextureCube> skybox;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;
    /** Opaque drawables visible in the main camera (frustum-culled). Used by GBuffer and forward opaque. */
    std::vector<Drawable> visibleOpaque;
    /** All opaque drawables (no main-camera cull). Used by shadow pass (per-cascade culling inside pass). */
    std::vector<Drawable> allOpaque;
    /** Transparent drawables (caller may sort). Used by transparency pass. */
    std::vector<Drawable> visibleTransparent;
};

} // namespace render
} // namespace eyelib
