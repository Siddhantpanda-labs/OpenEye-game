#pragma once

#include "api/particles/ParticleSystem.h"
#include "api/render/Camera.h"
#include "api/render/Mesh.h"

#include <memory>

namespace eyelib {
namespace api {

/**
 * @brief Build particle billboards into a mesh for rendering.
 */
class ParticleRenderer {
public:
    ParticleRenderer();

    std::shared_ptr<Mesh> BuildMesh(const ParticleSystem& system, EmitterHandle emitter,
                                    const Camera& camera);

private:
    std::shared_ptr<Mesh> m_mesh;
};

} // namespace api
} // namespace eyelib
