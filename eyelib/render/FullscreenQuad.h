#pragma once

#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace render {

/** Fullscreen quad (NDC) for lighting pass, post-processing, etc. */
class FullscreenQuad {
public:
    FullscreenQuad();
    ~FullscreenQuad() = default;

    FullscreenQuad(const FullscreenQuad&) = delete;
    FullscreenQuad& operator=(const FullscreenQuad&) = delete;

    /** Upload mesh to GPU. Call once after construction when device is available. */
    void UploadToGPU(rhi::IDevice* device);
    /** Draw fullscreen quad. Device must be non-null. */
    void Render(rhi::IDevice* device) const;
    bool IsValid() const { return m_mesh.IsValid(); }

private:
    void CreateQuad();

    MeshDescriptor m_descriptor;
    Mesh m_mesh;
};

} // namespace render
} // namespace eyelib
