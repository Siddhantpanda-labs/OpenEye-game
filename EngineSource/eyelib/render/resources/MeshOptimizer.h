#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

struct MeshDescriptor;

/**
 * @brief Mesh optimization: vertex cache, overdraw, LOD.
 */
class MeshOptimizer {
public:
    /** Reorder indices for better vertex cache locality. Operates on CPU descriptor. */
    static void OptimizeVertexCache(MeshDescriptor& desc);

    /** Generate LOD from descriptor (simplify); returns new Mesh if device non-null. GPU-only Mesh has no CPU data. */
    static std::shared_ptr<Mesh> GenerateLOD(const MeshDescriptor& desc, size_t targetVertexCount, rhi::IDevice* device = nullptr);
};

} // namespace render
} // namespace eyelib
