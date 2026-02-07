#pragma once

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

    void Render() const;
    bool IsValid() const { return m_mesh.IsValid(); }

private:
    void CreateQuad();

    Mesh m_mesh;
};

} // namespace render
} // namespace eyelib
