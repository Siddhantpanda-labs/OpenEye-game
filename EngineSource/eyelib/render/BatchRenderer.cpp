#include "eyelib/render/BatchRenderer.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>

namespace eyelib {
namespace render {

BatchRenderer::BatchRenderer() = default;

BatchRenderer::BatchRenderer(BatchRenderer&& other) noexcept
    : m_batches(std::move(other.m_batches)), m_sorted(other.m_sorted), m_device(other.m_device),
      m_shaderCache(other.m_shaderCache), m_pipelineStage(other.m_pipelineStage), m_cmd(other.m_cmd) {
    other.m_sorted = false;
    other.m_device = nullptr;
    other.m_cmd = nullptr;
}

BatchRenderer& BatchRenderer::operator=(BatchRenderer&& other) noexcept {
    if (this != &other) {
        m_batches = std::move(other.m_batches);
        m_sorted = other.m_sorted;
        m_device = other.m_device;
        m_shaderCache = other.m_shaderCache;
        m_pipelineStage = other.m_pipelineStage;
        m_cmd = other.m_cmd;
        other.m_sorted = false;
        other.m_device = nullptr;
        other.m_cmd = nullptr;
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

void BatchRenderer::SetShaderCache(ShaderCache* cache, PipelineStage stage) {
    m_shaderCache = cache;
    m_pipelineStage = stage;
}

void BatchRenderer::Flush() {
    if (m_batches.empty() || !m_device || !m_shaderCache) return;
    SortByMaterial();
    for (const auto& e : m_batches) {
        if (!e.mesh || !e.material) continue;
        ShaderKey skey{ m_pipelineStage, InstanceType::Static, e.material->GetMaterialShaderId() };
        auto shader = m_shaderCache->getOrCompile(m_device, skey, e.material.get());
        if (!shader || !shader->IsValid()) continue;
        Shader* sh = shader.get();
        sh->Bind(m_device);
        e.material->ApplyToShader(m_device, sh, m_cmd);
        sh->SetMat4("u_Model", e.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(e.transform)));
        e.mesh->Bind(m_device);
        { MeshHandle h = e.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        e.mesh->Unbind();
        sh->Unbind();
    }
}

} // namespace render
} // namespace eyelib
