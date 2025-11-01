#include "InstancedRenderer.h"
#include "resources/Shader.h"
#include "core/log/Log.h"
#include <glad/glad.h>
#include <algorithm>

namespace OpenEye {
namespace Rendering {

InstancedRenderer::InstancedRenderer() = default;

InstancedRenderer::~InstancedRenderer() {
    Clear();
}

InstancedRenderer::InstancedRenderer(InstancedRenderer&& other) noexcept
    : m_batches(std::move(other.m_batches))
    , m_totalInstances(other.m_totalInstances) {
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
    m_batches.clear();
    m_totalInstances = 0;
}

bool InstancedRenderer::AddInstance(std::shared_ptr<Mesh> mesh, 
                                   std::shared_ptr<Material> material,
                                   const glm::mat4& modelMatrix) {
    if (!mesh || !material) {
        return false;
    }
    
    BatchKey key;
    key.meshPtr = mesh.get();
    key.materialPtr = material.get();
    
    auto& batch = m_batches[key];
    
    // Initialize batch if new
    if (!batch.mesh) {
        batch.mesh = mesh;
        batch.material = material;
        batch.instances.reserve(1024); // Pre-allocate
    }
    
    // Check instance limit
    if (batch.instances.size() >= MAX_INSTANCES_PER_BATCH) {
        return false; // Batch full
    }
    
    // Add instance
    InstanceData instance;
    instance.modelMatrix = modelMatrix;
    batch.instances.push_back(instance);
    batch.bufferDirty = true;
    
    m_totalInstances++;
    return true;
}

void InstancedRenderer::UploadInstanceData(InstanceBatch& batch) {
    if (batch.instances.empty()) {
        return;
    }
    
    if (batch.instanceVBO == 0) {
        glGenBuffers(1, &batch.instanceVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 batch.instances.size() * sizeof(InstanceData),
                 batch.instances.data(),
                 GL_DYNAMIC_DRAW);
    
    batch.bufferDirty = false;
}

void InstancedRenderer::SetupInstanceAttributes(GLuint vao, GLuint instanceVBO) {
    if (vao == 0 || instanceVBO == 0) {
        return;
    }
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    
    // Model matrix is 4 vec4s (location 5-8 for columns)
    // Each column is a vec4, stride is size of InstanceData
    size_t baseOffset = offsetof(InstanceData, modelMatrix);
    
    // Column 0 (location 5)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                         reinterpret_cast<void*>(baseOffset + 0 * sizeof(glm::vec4)));
    glVertexAttribDivisor(5, 1);
    
    // Column 1 (location 6)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                         reinterpret_cast<void*>(baseOffset + 1 * sizeof(glm::vec4)));
    glVertexAttribDivisor(6, 1);
    
    // Column 2 (location 7)
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                         reinterpret_cast<void*>(baseOffset + 2 * sizeof(glm::vec4)));
    glVertexAttribDivisor(7, 1);
    
    // Column 3 (location 8)
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                         reinterpret_cast<void*>(baseOffset + 3 * sizeof(glm::vec4)));
    glVertexAttribDivisor(8, 1);
    
    glBindVertexArray(0);
}

void InstancedRenderer::RenderInstances(const glm::mat4& viewMatrix, 
                                       const glm::mat4& projectionMatrix,
                                       const glm::vec3& viewPos,
                                       const glm::vec3& lightDir,
                                       const glm::vec3& lightColor) {
    if (m_batches.empty()) {
        return;
    }
    
    // Set shared uniforms once (optimization)
    // These will be overridden per-material but set baseline
    
    for (auto& [key, batch] : m_batches) {
        if (batch.instances.empty() || !batch.mesh || !batch.material) {
            continue;
        }
        
        // Upload instance data if dirty
        if (batch.bufferDirty) {
            UploadInstanceData(batch);
        }
        
        // Bind material and set shared uniforms
        batch.material->Bind();
        auto shader = batch.material->GetShader();
        
        shader->SetMat4("u_View", viewMatrix);
        shader->SetMat4("u_Projection", projectionMatrix);
        shader->SetVec3("u_ViewPos", viewPos);
        shader->SetVec3("u_LightDir", lightDir);
        shader->SetVec3("u_LightColor", lightColor);
        
        // Setup instance attributes on mesh VAO
        batch.mesh->Bind();
        GLuint meshVAO = batch.mesh->GetVAO();
        
        // Upload and setup instance attributes
        glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
        SetupInstanceAttributes(meshVAO, batch.instanceVBO);
        
        // Draw instanced
        GLsizei indexCount = static_cast<GLsizei>(batch.mesh->GetIndexCount());
        GLsizei instanceCount = static_cast<GLsizei>(batch.instances.size());
        
        if (indexCount > 0 && instanceCount > 0) {
            glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
        } else if (instanceCount > 0) {
            // Draw arrays instanced (no indices)
            GLsizei vertexCount = static_cast<GLsizei>(batch.mesh->GetVertexCount());
            glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
        }
        
        // Disable instance attributes
        glDisableVertexAttribArray(5);
        glDisableVertexAttribArray(6);
        glDisableVertexAttribArray(7);
        glDisableVertexAttribArray(8);
        
        batch.mesh->Unbind();
        shader->Unbind();
    }
}

} // namespace Rendering
} // namespace OpenEye

