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
        case RST::DeferredLightingPass:
        case RST::IBLPass:
        case RST::OutlinePost:
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
        case RST::DeferredLightingPass:
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
