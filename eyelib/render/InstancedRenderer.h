#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {

struct InstanceData {
    math::Mat4 modelMatrix;
};

/**
 * @brief Instanced rendering. Groups by mesh/material, renders many instances efficiently.
 */
class InstancedRenderer {
public:
    InstancedRenderer();
    ~InstancedRenderer();

    InstancedRenderer(const InstancedRenderer&) = delete;
    InstancedRenderer& operator=(const InstancedRenderer&) = delete;
    InstancedRenderer(InstancedRenderer&& other) noexcept;
    InstancedRenderer& operator=(InstancedRenderer&& other) noexcept;

    void Clear();

    bool AddInstance(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4& transform);
    void SetInstanceData(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4* transforms, size_t count);
    void SetInstanceData(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const void* customData, size_t stride, size_t count);
    void Render(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, size_t instanceCount);
    void RenderAll();

    size_t GetBatchCount() const { return m_batches.size(); }
    size_t GetTotalInstanceCount() const { return m_totalInstances; }

    static constexpr size_t MAX_INSTANCES_PER_BATCH = 16384;

private:
    struct BatchKey {
        void* meshPtr;
        void* materialPtr;
        bool operator==(const BatchKey& o) const { return meshPtr == o.meshPtr && materialPtr == o.materialPtr; }
    };
    struct BatchKeyHash {
        size_t operator()(const BatchKey& k) const {
            return std::hash<void*>()(k.meshPtr) ^ (std::hash<void*>()(k.materialPtr) << 1);
        }
    };
    struct InstanceBatch {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        std::vector<InstanceData> instances;
        unsigned int instanceVBO = 0;
        bool bufferDirty = true;
    };

    std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> m_batches;
    size_t m_totalInstances = 0;

    void UploadInstanceData(InstanceBatch& batch);
    void SetupInstanceAttributes(unsigned int vao, unsigned int instanceVBO);
};

} // namespace render
} // namespace eyelib
