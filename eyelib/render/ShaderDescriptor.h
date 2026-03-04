#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/**
 * CPU-only shader definition (Phase 3: asset vs GPU instance).
 * No device or pipeline pointers; safe to store in registry and serialize.
 * Use with Shader::LoadFromDescriptor(device, desc) to create a GPU instance.
 */
struct ShaderDescriptor {
    /** Logical shader name (e.g. "pbr"); used when loadByName is true. */
    std::string shaderName;
    /** Vertex/fragment paths when loading by path instead of name. */
    std::string vertexPath;
    std::string fragmentPath;
    /** Keyword defines (e.g. "SKINNED", "SHADOWS") applied to source. */
    std::vector<std::string> keywords;
    /** Vertex layout and topology for pipeline creation. */
    rhi::VertexLayout layout{};
    rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList;
    /** True if this was loaded by name (Reload uses shaderName). */
    bool loadByName = false;
};

} // namespace render
} // namespace eyelib
