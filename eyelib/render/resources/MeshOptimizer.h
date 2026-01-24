#pragma once

#include "eyelib/render/resources/Mesh.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief Mesh optimization: vertex cache, overdraw, LOD.
 */
class MeshOptimizer {
public:
    /** Reorder indices for better vertex cache locality. Modifies mesh in place. */
    static void OptimizeVertexCache(Mesh& mesh);

    /** Generate LOD by simplifying mesh. Returns new mesh; original unchanged. */
    static std::shared_ptr<Mesh> GenerateLOD(const Mesh& mesh, size_t targetVertexCount);
};

} // namespace render
} // namespace eyelib
