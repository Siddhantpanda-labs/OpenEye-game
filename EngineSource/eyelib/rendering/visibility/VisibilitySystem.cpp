#include "eyelib/rendering/visibility/VisibilitySystem.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"

#include "eyelib/math/Frustum.h"
#include "eyelib/math/Types.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace eyelib {
namespace rendering {
namespace {

uint32_t QuantizeDistance(float dist) {
    return static_cast<uint32_t>(glm::clamp(dist * 1000.0f, 0.0f, float(0xFFFFFEu)));
}

/**
 * Layer in high bits (opaque … sky), then material, then depth bucket.
 * Opaque / alpha-test: nearer instances sort first within a material (forward Z).
 * Transparent: farther first within a material (back-to-front overdraw).
 */
uint64_t ComputeSortKey(DrawLayer layer, uint32_t materialId, float distanceFromCamera) {
    const uint32_t q = QuantizeDistance(distanceFromCamera);
    uint32_t depthBits = q;
    if (layer == DrawLayer::Transparent)
        depthBits = 0xFFFFFEu - q;
    if (layer == DrawLayer::Sky)
        depthBits = 0;

    constexpr uint64_t kLayerShift = 48;
    constexpr uint64_t kMatShift = 32;
    return (uint64_t{static_cast<uint8_t>(layer)} << kLayerShift) |
           (uint64_t{materialId} << kMatShift) | uint64_t{depthBits};
}

}  // namespace

void VisibilitySystem::BuildCpuDrawList(const IRenderSceneView* scene, DrawList& out) const {
    out.Clear();
    if (!scene)
        return;

    const CameraView* cam = scene->TryGetCamera();
    if (!cam)
        return;

    const size_t n = scene->SceneDrawInstanceCount();
    const SceneDrawInstance* instances = scene->SceneDrawInstances();
    if (n == 0 || !instances)
        return;

    math::Frustum frustum;
    frustum.FromMatrix(cam->projection * cam->view);

    const math::Vec3 viewPos = cam->viewPos;

    out.items.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const SceneDrawInstance& src = instances[i];
        if (!src.localBounds.IsValid())
            continue;

        const math::AABB worldAabb = src.localBounds.Transform(src.worldTransform);
        if (frustum.Contains(worldAabb) == 0)
            continue;

        const math::Vec3 center = worldAabb.GetCenter();
        const float dist = glm::length(center - viewPos);

        DrawItem item{};
        item.meshHandle = src.meshHandle;
        item.materialId = src.materialId;
        item.transformIndex = src.transformIndex;
        item.worldMatrix = src.worldTransform;
        item.flags = src.flags;
        item.layer = src.layer;
        item.sortKey = ComputeSortKey(src.layer, src.materialId, dist);
        out.items.push_back(item);
    }

    std::sort(out.items.begin(), out.items.end(),
              [](const DrawItem& a, const DrawItem& b) { return a.sortKey < b.sortKey; });
}

}  // namespace rendering
}  // namespace eyelib
