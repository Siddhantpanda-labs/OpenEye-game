#include "eyelib/render/FullscreenQuad.h"

namespace eyelib {
namespace render {

FullscreenQuad::FullscreenQuad() {
    CreateQuad();
}

void FullscreenQuad::CreateQuad() {
    m_descriptor.vertices.resize(4);
    m_descriptor.vertices[0].position = math::Vec3(-1.0f, -1.0f, 0.0f);
    m_descriptor.vertices[0].uv = math::Vec2(0.0f, 0.0f);
    m_descriptor.vertices[1].position = math::Vec3(1.0f, -1.0f, 0.0f);
    m_descriptor.vertices[1].uv = math::Vec2(1.0f, 0.0f);
    m_descriptor.vertices[2].position = math::Vec3(1.0f, 1.0f, 0.0f);
    m_descriptor.vertices[2].uv = math::Vec2(1.0f, 1.0f);
    m_descriptor.vertices[3].position = math::Vec3(-1.0f, 1.0f, 0.0f);
    m_descriptor.vertices[3].uv = math::Vec2(0.0f, 1.0f);
    m_descriptor.indices32 = {0, 1, 2, 2, 3, 0};
    m_descriptor.indices16.clear();
    m_descriptor.indexFormat = IndexFormat::UInt32;
    m_descriptor.submeshes.clear();
}

void FullscreenQuad::UploadToGPU(rhi::IDevice* device) {
    if (device && !m_descriptor.vertices.empty()) m_mesh.CreateFromDescriptor(device, m_descriptor);
}

void FullscreenQuad::Render(rhi::IDevice* device) const {
    if (!device || !m_mesh.IsValid()) return;
    m_mesh.Draw(device);
}

} // namespace render
} // namespace eyelib
