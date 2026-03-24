#include "eyelib/render/rhi/ResourceSetLayoutDesc.h"

namespace eyelib {
namespace render {
namespace rhi {

using ResourceSetType = IDevice::ResourceSetType;

PipelineLayoutDesc MakeResourceSetDesc(ResourceSetType type) {
    PipelineLayoutDesc desc;
    switch (type) {
    case ResourceSetType::Frame:
        desc.bindings = {
            {0, BindingType::UniformBuffer},
            {1, BindingType::UniformBuffer},
        };
        break;
    case ResourceSetType::PostPass:
        desc.bindings = {
            {0, BindingType::Texture},
            {1, BindingType::Texture},
            {2, BindingType::Texture},
            {3, BindingType::Texture},
            {4, BindingType::Texture},
            {5, BindingType::Texture},
            {6, BindingType::Texture},
        };
        break;
    case ResourceSetType::ForwardPass:
        desc.bindings = {
            {0, BindingType::Texture},
            {1, BindingType::StorageBuffer},
            {2, BindingType::StorageBuffer},
            {3, BindingType::Texture},
            {4, BindingType::Texture},
            {5, BindingType::Texture},
        };
        break;
    case ResourceSetType::ShadowPass:
        desc.bindings = {{0, BindingType::UniformBuffer}};
        break;
    case ResourceSetType::LightCullPass:
        desc.bindings = {
            {0, BindingType::UniformBuffer},
            {1, BindingType::StorageBuffer},
        };
        break;
    case ResourceSetType::Material:
        desc.bindings = {
            {0, BindingType::UniformBuffer},
            {1, BindingType::Texture},
            {2, BindingType::Texture},
            {3, BindingType::Texture},
        };
        break;
    case ResourceSetType::SceneBuffers:
        desc.bindings = {
            {0, BindingType::StorageBuffer},
            {1, BindingType::StorageBuffer},
            {2, BindingType::StorageBuffer},
            {3, BindingType::StorageBuffer},
        };
        break;
    case ResourceSetType::DeferredLightingPass:
        desc.bindings = {
            {0, BindingType::Texture},
            {1, BindingType::Texture},
            {2, BindingType::Texture},
            {3, BindingType::Texture},
            {4, BindingType::Texture},
            {5, BindingType::StorageBuffer},
            {6, BindingType::UniformBuffer},
            {7, BindingType::Texture},
            {8, BindingType::Texture},
            {9, BindingType::Texture},
        };
        break;
    case ResourceSetType::IBLPass:
        desc.bindings = {{0, BindingType::TextureCube}};
        break;
    case ResourceSetType::OutlinePost:
        desc.bindings = {
            {0, BindingType::Texture},
            {1, BindingType::Texture},
        };
        break;
    }
    return desc;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
