#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

/**
 * CPU-only shader definition (Phase 3: asset vs GPU instance).
 * No device or pipeline pointers; safe to store in registry and serialize.
 * Use with Shader::LoadFromDescriptor(device, desc) to create a GPU instance.
 */
struct ShaderDescriptor {
    /** Logical shader asset path without extension (e.g. passes/forward/forward); used when loadFromAsset is true. */
    std::string assetPath;
    /** Vertex/fragment paths when loading by path instead of asset path. */
    std::string vertexPath;
    std::string fragmentPath;
    /** Keyword defines (e.g. "SKINNED", "SHADOWS") applied to source. */
    std::vector<std::string> keywords;
    /** Vertex layout and topology for pipeline creation. */
    rhi::VertexLayout layout{};
    rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList;
    /** True if Reload should use assetPath + layout/topology/keywords. */
    bool loadFromAsset = false;
    /** Pipeline layout kind (PushOnly, Post, Forward, etc.). Used when creating pipeline from name. */
    rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly;
    /** Vulkan dynamic rendering attachment formats; unset = infer from layoutKind at create time. */
    std::optional<rhi::PipelineAttachmentFormats> attachmentFormats;
};

} // namespace rendering
} // namespace eyelib
