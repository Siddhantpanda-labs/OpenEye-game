#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"
#include "eyelib/rendering/scene/DrawList.h"

namespace eyelib {
namespace rendering {

/** One drawable instance in the scene view (CPU); consumed by `VisibilitySystem` for frustum cull → `DrawItem`. */
struct SceneDrawInstance {
    uint32_t meshHandle = 0;
    uint32_t materialId = 0;
    int32_t submeshIndex = -1;
    uint32_t transformIndex = 0;
    math::Mat4 worldTransform{1.0f};
    /** Model-space bounds; transformed to world each frame for culling. */
    math::AABB localBounds{};
    DrawLayer layer = DrawLayer::Opaque;
    uint32_t flags = draw_item_flags::kNone;
};

}  // namespace rendering
}  // namespace eyelib
