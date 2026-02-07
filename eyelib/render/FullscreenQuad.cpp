#include "eyelib/render/FullscreenQuad.h"

namespace eyelib {
namespace render {

FullscreenQuad::FullscreenQuad() {
    CreateQuad();
}

void FullscreenQuad::CreateQuad() {
    std::vector<Vertex> verts(4);
    verts[0].position = math::Vec3(-1.0f, -1.0f, 0.0f);
    verts[0].uv = math::Vec2(0.0f, 0.0f);
    verts[1].position = math::Vec3(1.0f, -1.0f, 0.0f);
    verts[1].uv = math::Vec2(1.0f, 0.0f);
    verts[2].position = math::Vec3(1.0f, 1.0f, 0.0f);
    verts[2].uv = math::Vec2(1.0f, 1.0f);
    verts[3].position = math::Vec3(-1.0f, 1.0f, 0.0f);
    verts[3].uv = math::Vec2(0.0f, 1.0f);

    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    m_mesh.SetVertices(verts);
    m_mesh.SetIndices(indices);
    m_mesh.UploadToGPU();
}

void FullscreenQuad::Render() const {
    if (!m_mesh.IsValid()) return;
    m_mesh.Draw();
}

} // namespace render
} // namespace eyelib
