#pragma once

#include "eyelib/render/ShaderKey.h"

#include <string>

namespace eyelib {
namespace render {

/**
 * Resolve logical shader template name for a pass/instance pair.
 * preferStaticGBufferTemplate:
 * - true:  static GBuffer -> passes/deferred/gbuffer
 * - false: static GBuffer -> passes/deferred/gbuffer_skinned (legacy material-template behavior)
 */
inline std::string ResolveShaderTemplateName(
    PipelineStage stage,
    InstanceType instanceType,
    bool preferStaticGBufferTemplate) {
    switch (stage) {
    case PipelineStage::GBuffer:
        if (instanceType == InstanceType::Instanced) return "passes/deferred/gbuffer_instanced";
        if (instanceType == InstanceType::Skinned) return "passes/deferred/gbuffer_skinned";
        return preferStaticGBufferTemplate ? "passes/deferred/gbuffer" : "passes/deferred/gbuffer_skinned";
    case PipelineStage::Depth:
        if (instanceType == InstanceType::Instanced) return "passes/shadow/shadow_instanced";
        return (instanceType == InstanceType::Skinned) ? "passes/shadow/shadow_skinned" : "passes/shadow/shadow";
    case PipelineStage::Forward:
        if (instanceType == InstanceType::Instanced) return {};  // no forward_instanced template
        return (instanceType == InstanceType::Skinned) ? "passes/forward/forward_skinned" : "passes/forward/forward";
    case PipelineStage::ForwardPlus:
        if (instanceType == InstanceType::Instanced) return "passes/forward/forwardplus_instanced";
        return (instanceType == InstanceType::Skinned) ? "passes/forward/forwardplus_skinned" : "passes/forward/forwardplus";
    default:
        return {};
    }
}

} // namespace render
} // namespace eyelib

