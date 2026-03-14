#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

/**
 * One batch of instances: same mesh + submesh + effective material payload.
 * Used by the submission layer to reduce draw calls via instancing.
 * Skinned drawables are not grouped; they form single-instance batches.
 */
struct DrawBatch {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    std::shared_ptr<MaterialInstance> materialInstance;
    int32_t submeshIndex = -1;
    std::vector<math::Mat4> transforms;

    /** Number of instances in this batch. */
    size_t InstanceCount() const { return transforms.size(); }
    /** True if this batch can be drawn with one instanced call. */
    bool IsInstanced() const { return transforms.size() > 1u; }
    /** Build a single Drawable for the i-th instance (for single-draw path). */
    Drawable ToDrawable(size_t i) const;
};

/**
 * Build batches from drawables: group by (mesh, base material, material-instance payload, submeshIndex).
 * Skinned drawables (bones) are not grouped; each becomes a single-instance batch.
 * Returns batches in stable order; each batch has at least one transform.
 */
std::vector<DrawBatch> BuildBatches(const std::vector<Drawable>& drawables);

} // namespace render
} // namespace eyelib
