#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

namespace rhi { struct IDevice; struct IBuffer; }

/** Phase 5: std140-compatible header for scene lighting SSBO. Matches GLSL layout. */
struct SceneLightHeader {
    math::Vec3 direction{0.5f, 1.0f, 0.3f};
    float pad0 = 0.0f;
    math::Vec3 color{1.0f, 0.95f, 0.9f};
    float pad1 = 0.0f;
    float intensity = 3.0f;
    float pad2 = 0.0f, pad3 = 0.0f, pad4 = 0.0f;
    math::Vec3 ambient{0.08f};
    float pad5 = 0.0f;
    float exposure = 1.0f;
    float pad6 = 0.0f, pad7 = 0.0f, pad8 = 0.0f;
    math::Vec3 viewPos{0, 0, 0};
    float pad9 = 0.0f;
    math::Mat4 lightSpaceMatrix{1.0f};
    float shadowBias = 0.005f;
    float pad10 = 0.0f, pad11 = 0.0f, pad12 = 0.0f;
    int32_t useShadows = 0;
    float pad13 = 0.0f, pad14 = 0.0f, pad15 = 0.0f;
    int32_t pointLightCount = 0;
    int32_t spotLightCount = 0;
    float pad16 = 0.0f, pad17 = 0.0f;
    int32_t cascadeCount = 0;  // 0 = no cascade, 1 = single, 4 = CSM
    float pad18 = 0.0f, pad19 = 0.0f, pad20 = 0.0f;
    math::Vec4 farBounds{0.0f, 0.0f, 0.0f, 0.0f};  // NDC far bounds per cascade for selection
};

/** Deferred lighting pass uses up to 8 point + 4 spot; buffer sized for that. */
constexpr uint32_t kSceneLightingMaxPointLights = 8u;
constexpr uint32_t kSceneLightingMaxSpotLights = 4u;

constexpr size_t kSceneLightHeaderSize = 240u;  // std140 layout (includes cascadeCount + farBounds)
constexpr size_t kSceneLightingBufferSize = kSceneLightHeaderSize
    + kSceneLightingMaxPointLights * sizeof(PointLight)
    + kSceneLightingMaxSpotLights * sizeof(SpotLight);

class CascadeShadowMap;
/** Fill the scene lighting buffer: header from light/view/shadow, then point lights, then spot lights. cascadeMap optional for cascade count and farBounds. */
void FillSceneLightingBuffer(rhi::IBuffer* buffer,
    const LightParams& light,
    const math::Vec3& viewPos,
    const math::Mat4& lightSpaceMatrix,
    float shadowBias,
    bool useShadows,
    const std::vector<PointLight>* pointLights,
    const std::vector<SpotLight>* spotLights,
    const CascadeShadowMap* cascadeMap = nullptr);

} // namespace render
} // namespace eyelib
