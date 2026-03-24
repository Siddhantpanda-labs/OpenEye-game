#include "eyelib/render/rhi/OpenGL/OpenGLResourceSpaceBindings.h"

namespace eyelib {
namespace render {
namespace rhi {

using RST = IDevice::ResourceSetType;

uint32_t GLBindingForResourceSet(RST set, BindingType type, uint32_t logical) {
    switch (type) {
    case BindingType::UniformBuffer:
        switch (set) {
        case RST::Frame:
            return logical;
        case RST::Material:
            if (logical == 0) return 2u;
            break;
        case RST::ShadowPass:
            if (logical == 0) return 2u;
            break;
        case RST::DeferredLightingPass:
            if (logical == 6u) return 8u;
            break;
        default:
            break;
        }
        return logical;
    case BindingType::Texture:
    case BindingType::TextureCube:
        switch (set) {
        case RST::ForwardPass:
            return logical;
        case RST::Material:
            if (logical >= 1u && logical <= 3u) return 5u + logical;
            break;
        case RST::PostPass:
        case RST::IBLPass:
        case RST::OutlinePost:
            return logical;
        case RST::DeferredLightingPass:
            // Frame UBOs occupy GL UBO binding points 0–1; fragment samplers cannot share those layout(binding)
            // indices with gbuffer textures. Offset pass textures: logical 0–4 -> physical 2–6; 7–9 -> 9–11.
            if (logical <= 4u) return 2u + logical;
            if (logical >= 7u && logical <= 9u) return logical;
            return logical;
        default:
            break;
        }
        return logical;
    case BindingType::StorageBuffer:
        switch (set) {
        case RST::ForwardPass:
            if (logical == 1u || logical == 2u) return 9u + logical;
            break;
        case RST::SceneBuffers:
            return logical;
        case RST::LightCullPass:
            return logical;
        case RST::DeferredLightingPass:
            if (logical == 5u) return 7u;
            return logical;
        default:
            break;
        }
        return logical;
    }
    return logical;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
