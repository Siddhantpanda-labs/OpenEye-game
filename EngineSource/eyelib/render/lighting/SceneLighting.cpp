#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/rhi/RHI.h"
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {

void FillSceneLightingBuffer(rhi::IBuffer* buffer,
    const LightParams& light,
    const math::Vec3& viewPos,
    const math::Mat4& lightSpaceMatrix,
    float shadowBias,
    bool useShadows,
    const std::vector<PointLight>* pointLights,
    const std::vector<SpotLight>* spotLights,
    const CascadeShadowMap* cascadeMap) {
    if (!buffer) return;

    SceneLightHeader header;
    header.direction = light.direction;
    header.color = light.color;
    header.intensity = light.intensity;
    header.ambient = light.ambient;
    header.exposure = light.exposure;
    header.viewPos = viewPos;
    header.lightSpaceMatrix = lightSpaceMatrix;
    header.shadowBias = shadowBias;
    header.useShadows = useShadows ? 1 : 0;
    header.pointLightCount = pointLights ? static_cast<int32_t>(std::min(pointLights->size(), size_t(kSceneLightingMaxPointLights))) : 0;
    header.spotLightCount = spotLights ? static_cast<int32_t>(std::min(spotLights->size(), size_t(kSceneLightingMaxSpotLights))) : 0;
    if (cascadeMap && useShadows) {
        header.cascadeCount = static_cast<int32_t>(cascadeMap->GetSplitCount());
        const auto& fb = cascadeMap->GetFarBounds();
        for (size_t i = 0; i < 4u && i < fb.size(); ++i)
            header.farBounds[static_cast<int>(i)] = fb[i];
    }

    size_t offset = 0;
    buffer->Update(&header, offset, kSceneLightHeaderSize);
    offset += kSceneLightHeaderSize;

    if (pointLights && !pointLights->empty()) {
        const size_t count = static_cast<size_t>(header.pointLightCount);
        buffer->Update(pointLights->data(), offset, count * sizeof(PointLight));
    }
    offset += kSceneLightingMaxPointLights * sizeof(PointLight);

    if (spotLights && !spotLights->empty()) {
        const size_t count = static_cast<size_t>(header.spotLightCount);
        buffer->Update(spotLights->data(), offset, count * sizeof(SpotLight));
    }
}

static_assert(sizeof(SceneLightHeader) == kSceneLightHeaderSize, "SceneLightHeader must match GLSL std140 layout");

} // namespace render
} // namespace eyelib
