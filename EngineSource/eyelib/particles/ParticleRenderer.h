#pragma once

#include "eyelib/particles/ParticleSystem.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Mesh.h"

#include <memory>

namespace eyelib {
namespace render { namespace rhi { struct IDevice; } }
namespace particles {

/**
 * @brief CPU particle renderer that builds billboards into a mesh.
 * BuildMesh fills a descriptor and creates a GPU mesh via Mesh::Create(device, desc).
 */
class ParticleRenderer {
public:
    ParticleRenderer() = default;

    void SetMaterial(const std::shared_ptr<render::Material>& material);
    std::shared_ptr<render::Material> GetMaterial() const { return m_material; }

    /** Build particle billboards into a GPU mesh. Device required for creation. */
    std::shared_ptr<render::Mesh> BuildMesh(render::rhi::IDevice* device,
                                            const ParticleSystem& system, EmitterHandle emitter,
                                            const render::CameraView& camera);

private:
    std::shared_ptr<render::Material> m_material;
};

} // namespace particles
} // namespace eyelib
