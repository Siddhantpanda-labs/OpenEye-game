#include "eyelib/render/InstancedRenderer.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {

InstancedRenderer::InstancedRenderer() = default;

InstancedRenderer::~InstancedRenderer() {
    Clear();
}

InstancedRenderer::InstancedRenderer(InstancedRenderer&& other) noexcept
    : m_batches(std::move(other.m_batches)), m_totalInstances(other.m_totalInstances), m_device(other.m_device),
      m_shaderCache(other.m_shaderCache), m_pipelineStage(other.m_pipelineStage), m_cmd(other.m_cmd) {
    other.m_totalInstances = 0;
    other.m_device = nullptr;
    other.m_cmd = nullptr;
}

InstancedRenderer& InstancedRenderer::operator=(InstancedRenderer&& other) noexcept {
    if (this != &other) {
        Clear();
        m_batches = std::move(other.m_batches);
        m_totalInstances = other.m_totalInstances;
        m_device = other.m_device;
        m_shaderCache = other.m_shaderCache;
        m_pipelineStage = other.m_pipelineStage;
        m_cmd = other.m_cmd;
        other.m_totalInstances = 0;
        other.m_device = nullptr;
        other.m_cmd = nullptr;
    }
    return *this;
}

void InstancedRenderer::Clear() {
    m_batches.clear();
    m_totalInstances = 0;
}

bool InstancedRenderer::AddInstance(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4& transform) {
    if (!mesh || !material) return false;
    BatchKey key{mesh.get(), material.get()};
    auto& batch = m_batches[key];
    if (!batch.mesh) {
        batch.mesh = mesh;
        batch.material = material;
        batch.instances.reserve(1024);
    }
    if (batch.instances.size() >= MAX_INSTANCES_PER_BATCH) return false;
    InstanceData inst;
    inst.modelMatrix = transform;
    batch.instances.push_back(inst);
    ++m_totalInstances;
    return true;
}

void InstancedRenderer::SetInstanceData(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4* transforms, size_t count) {
    if (!mesh || !material || !transforms || count == 0) return;
    BatchKey key{mesh.get(), material.get()};
    auto& batch = m_batches[key];
    if (!batch.mesh) {
        batch.mesh = mesh;
        batch.material = material;
    }
    batch.instances.clear();
    batch.instances.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        InstanceData inst;
        inst.modelMatrix = transforms[i];
        batch.instances.push_back(inst);
    }
    m_totalInstances = 0;
    for (const auto& [k, b] : m_batches) m_totalInstances += b.instances.size();
}

void InstancedRenderer::SetInstanceData(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const void* customData, size_t stride, size_t count) {
    (void)mesh;
    (void)material;
    (void)customData;
    (void)stride;
    (void)count;
    /* Stub: would set custom per-instance data (e.g. color tint per instance). */
}

void InstancedRenderer::SetShaderCache(ShaderCache* cache, PipelineStage stage) {
    m_shaderCache = cache;
    m_pipelineStage = stage;
}

void InstancedRenderer::Render(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, size_t instanceCount) {
    if (!m_device || !m_shaderCache || !mesh || !material || instanceCount == 0) return;
    BatchKey key{mesh.get(), material.get()};
    auto it = m_batches.find(key);
    if (it == m_batches.end() || it->second.instances.size() < instanceCount) return;
    auto& batch = it->second;
    ShaderKey skey{ m_pipelineStage, InstanceType::Static, batch.material->GetMaterialShaderId() };
    auto shader = m_shaderCache->getOrCompile(m_device, skey, batch.material.get());
    if (!shader || !shader->IsValid()) return;
    Shader* sh = shader.get();
    sh->Bind(m_device);
        batch.material->ApplyToShader(m_device, sh, m_cmd);
        const bool vkFwd = m_cmd && backend::UseTypedPushConstants(m_device);
        const int albedoFlag = ComputeForwardAlbedoTextureFlag(batch.material.get(), nullptr);
        for (size_t i = 0; i < instanceCount; ++i) {
            if (vkFwd)
                abi::WriteForwardPushConstants(m_cmd, batch.instances[i].modelMatrix, 0, albedoFlag);
            else {
                sh->SetMat4("u_Model", batch.instances[i].modelMatrix);
                sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(batch.instances[i].modelMatrix)));
            }
            { MeshHandle h = batch.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        }
    sh->Unbind();
}

void InstancedRenderer::RenderAll() {
    if (!m_device || !m_shaderCache) return;
    for (auto& [k, batch] : m_batches) {
        if (batch.instances.empty() || !batch.mesh || !batch.material) continue;
        ShaderKey skey{ m_pipelineStage, InstanceType::Static, batch.material->GetMaterialShaderId() };
        auto shader = m_shaderCache->getOrCompile(m_device, skey, batch.material.get());
        if (!shader || !shader->IsValid()) continue;
        Shader* sh = shader.get();
        sh->Bind(m_device);
        batch.material->ApplyToShader(m_device, sh, m_cmd);
        const bool vkFwd = m_cmd && backend::UseTypedPushConstants(m_device);
        const int albedoFlag = ComputeForwardAlbedoTextureFlag(batch.material.get(), nullptr);
        for (const auto& inst : batch.instances) {
            if (vkFwd)
                abi::WriteForwardPushConstants(m_cmd, inst.modelMatrix, 0, albedoFlag);
            else {
                sh->SetMat4("u_Model", inst.modelMatrix);
                sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(inst.modelMatrix)));
            }
            { MeshHandle h = batch.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        }
        sh->Unbind();
    }
}

} // namespace render
} // namespace eyelib
