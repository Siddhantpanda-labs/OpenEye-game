#pragma once

#include "eyelib/particles/ParticleSystem.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Mesh.h"

#include <memory>

namespace eyelib {
namespace particles {

/**
 * @brief CPU particle renderer that builds billboards into a mesh.
 */
class ParticleRenderer {
public:
    ParticleRenderer();

    void SetMaterial(const std::shared_ptr<render::Material>& material);
    std::shared_ptr<render::Material> GetMaterial() const { return m_material; }

    std::shared_ptr<render::Mesh> BuildMesh(const ParticleSystem& system, EmitterHandle emitter,
                                            const render::CameraView& camera);

private:
    std::shared_ptr<render::Mesh> m_mesh;
    std::shared_ptr<render::Material> m_material;
};

} // namespace particles
} // namespace eyelib
