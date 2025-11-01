#pragma once

#include "resources/Mesh.h"
#include "resources/Material.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace OpenEye {
namespace Rendering {

// Instance data for instanced rendering
struct InstanceData {
    glm::mat4 modelMatrix;
    // Can add more per-instance data here (color tint, etc.)
};

// Groups instances by mesh/material combination for efficient batching
class InstancedRenderer {
public:
    InstancedRenderer();
    ~InstancedRenderer();
    
    // Non-copyable
    InstancedRenderer(const InstancedRenderer&) = delete;
    InstancedRenderer& operator=(const InstancedRenderer&) = delete;
    
    // Movable
    InstancedRenderer(InstancedRenderer&& other) noexcept;
    InstancedRenderer& operator=(InstancedRenderer&& other) noexcept;
    
    // Clear all instance groups
    void Clear();
    
    // Add an instance to be rendered
    // Returns true if added, false if instance limit reached
    bool AddInstance(std::shared_ptr<Mesh> mesh, 
                    std::shared_ptr<Material> material,
                    const glm::mat4& modelMatrix);
    
    // Render all instances (batched by mesh/material)
    void RenderInstances(const glm::mat4& viewMatrix, 
                        const glm::mat4& projectionMatrix,
                        const glm::vec3& viewPos,
                        const glm::vec3& lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)),
                        const glm::vec3& lightColor = glm::vec3(1.0f));
    
    // Get statistics
    size_t GetBatchCount() const { return m_batches.size(); }
    size_t GetTotalInstanceCount() const { return m_totalInstances; }
    
    // Maximum instances per batch (OpenGL limits vary, 16384 is safe)
    static constexpr size_t MAX_INSTANCES_PER_BATCH = 16384;

private:
    // Key for grouping instances by mesh/material
    struct BatchKey {
        void* meshPtr;
        void* materialPtr;
        
        bool operator==(const BatchKey& other) const {
            return meshPtr == other.meshPtr && materialPtr == other.materialPtr;
        }
    };
    
    struct BatchKeyHash {
        size_t operator()(const BatchKey& key) const {
            return std::hash<void*>()(key.meshPtr) ^ (std::hash<void*>()(key.materialPtr) << 1);
        }
    };
    
    // Batch of instances sharing same mesh/material
    struct InstanceBatch {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        std::vector<InstanceData> instances;
        
        // OpenGL buffers for instancing
        GLuint instanceVBO = 0;
        bool bufferDirty = true;
        
        ~InstanceBatch() {
            if (instanceVBO != 0) {
                glDeleteBuffers(1, &instanceVBO);
            }
        }
    };
    
    std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> m_batches;
    size_t m_totalInstances = 0;
    
    // Upload instance data to GPU
    void UploadInstanceData(InstanceBatch& batch);
    
    // Setup instance attributes (model matrix)
    void SetupInstanceAttributes(GLuint vao, GLuint instanceVBO);
};

} // namespace Rendering
} // namespace OpenEye

