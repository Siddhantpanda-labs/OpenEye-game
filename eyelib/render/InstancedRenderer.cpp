#include "eyelib/render/InstancedRenderer.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {

InstancedRenderer::InstancedRenderer() = default;

InstancedRenderer::~InstancedRenderer() {
    Clear();
}

InstancedRenderer::InstancedRenderer(InstancedRenderer&& other) noexcept
    : m_batches(std::move(other.m_batches)), m_totalInstances(other.m_totalInstances) {
    other.m_totalInstances = 0;
}

InstancedRenderer& InstancedRenderer::operator=(InstancedRenderer&& other) noexcept {
    if (this != &other) {
        Clear();
        m_batches = std::move(other.m_batches);
        m_totalInstances = other.m_totalInstances;
        other.m_totalInstances = 0;
    }
    return *this;
}

void InstancedRenderer::Clear() {
    for (auto& [k, b] : m_batches) {
        if (b.instanceVBO) { glDeleteBuffers(1, &b.instanceVBO); b.instanceVBO = 0; }
    }
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
    batch.bufferDirty = true;
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
    batch.bufferDirty = true;
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

void InstancedRenderer::UploadInstanceData(InstanceBatch& batch) {
    if (batch.instances.empty()) return;
    if (!batch.instanceVBO) glGenBuffers(1, &batch.instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.instances.size() * sizeof(InstanceData)),
                 batch.instances.data(), GL_DYNAMIC_DRAW);
    batch.bufferDirty = false;
}

void InstancedRenderer::SetupInstanceAttributes(unsigned int vao, unsigned int instanceVBO) {
    if (!vao || !instanceVBO) return;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    size_t off = offsetof(InstanceData, modelMatrix);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(7 + i);
        glVertexAttribPointer(7 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                             reinterpret_cast<void*>(off + i * sizeof(math::Vec4)));
        glVertexAttribDivisor(7 + i, 1);
    }
    glBindVertexArray(0);
}

void InstancedRenderer::Render(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, size_t instanceCount) {
    if (!mesh || !material || instanceCount == 0) return;
    BatchKey key{mesh.get(), material.get()};
    auto it = m_batches.find(key);
    if (it == m_batches.end() || it->second.instances.size() < instanceCount) return;
    auto& batch = it->second;
    if (batch.bufferDirty) UploadInstanceData(batch);
    batch.material->Bind();
    batch.mesh->Bind();
    SetupInstanceAttributes(batch.mesh->GetVAO(), batch.instanceVBO);
    GLenum idxType = (batch.mesh->GetIndexFormat() == IndexFormat::UInt32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    GLsizei idxCnt = static_cast<GLsizei>(batch.mesh->GetIndexCount());
    if (idxCnt > 0)
        glDrawElementsInstanced(GL_TRIANGLES, idxCnt, idxType, nullptr, static_cast<GLsizei>(instanceCount));
    else
        glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.mesh->GetVertexCount()), static_cast<GLsizei>(instanceCount));
    for (int i = 0; i < 4; ++i) glDisableVertexAttribArray(7 + i);
    batch.mesh->Unbind();
    Shader* sh = batch.material->GetShader().get();
    if (sh) sh->Unbind();
}

void InstancedRenderer::RenderAll() {
    for (auto& [k, batch] : m_batches) {
        if (batch.instances.empty() || !batch.mesh || !batch.material) continue;
        if (batch.bufferDirty) UploadInstanceData(batch);
        batch.material->Bind();
        batch.mesh->Bind();
        SetupInstanceAttributes(batch.mesh->GetVAO(), batch.instanceVBO);
        GLenum idxType = (batch.mesh->GetIndexFormat() == IndexFormat::UInt32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
        GLsizei idxCnt = static_cast<GLsizei>(batch.mesh->GetIndexCount());
        GLsizei instCnt = static_cast<GLsizei>(batch.instances.size());
        if (idxCnt > 0)
            glDrawElementsInstanced(GL_TRIANGLES, idxCnt, idxType, nullptr, instCnt);
        else
            glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.mesh->GetVertexCount()), instCnt);
        for (int i = 0; i < 4; ++i) glDisableVertexAttribArray(7 + i);
        batch.mesh->Unbind();
        Shader* sh = batch.material->GetShader().get();
        if (sh) sh->Unbind();
    }
}

} // namespace render
} // namespace eyelib
