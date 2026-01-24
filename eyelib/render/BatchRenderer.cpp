#include "eyelib/render/BatchRenderer.h"
#include "eyelib/render/resources/Shader.h"
#include <algorithm>

namespace eyelib {
namespace render {

BatchRenderer::BatchRenderer() = default;

BatchRenderer::BatchRenderer(BatchRenderer&& other) noexcept : m_batches(std::move(other.m_batches)), m_sorted(other.m_sorted) {
    other.m_sorted = false;
}

BatchRenderer& BatchRenderer::operator=(BatchRenderer&& other) noexcept {
    if (this != &other) {
        m_batches = std::move(other.m_batches);
        m_sorted = other.m_sorted;
        other.m_sorted = false;
    }
    return *this;
}

void BatchRenderer::AddMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4& transform) {
    if (!mesh || !material) return;
    BatchEntry e;
    e.mesh = mesh;
    e.material = material;
    e.transform = transform;
    e.materialHash = material->GetHash();
    m_batches.push_back(e);
    m_sorted = false;
}

void BatchRenderer::Clear() {
    m_batches.clear();
    m_sorted = false;
}

void BatchRenderer::SortByMaterial() {
    if (m_sorted) return;
    std::sort(m_batches.begin(), m_batches.end(), [](const BatchEntry& a, const BatchEntry& b) {
        return a.materialHash < b.materialHash;
    });
    m_sorted = true;
}

void BatchRenderer::Flush() {
    if (m_batches.empty()) return;
    SortByMaterial();
    for (const auto& e : m_batches) {
        if (!e.mesh || !e.material) continue;
        e.material->Bind();
        e.mesh->Bind();
        Shader* sh = e.material->GetShader().get();
        if (sh) {
            sh->SetMat4("u_Model", e.transform);
            e.mesh->Draw();
            e.mesh->Unbind();
            sh->Unbind();
        }
    }
}

} // namespace render
} // namespace eyelib
