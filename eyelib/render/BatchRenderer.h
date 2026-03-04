#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief Batch renderer. Combines meshes (static/dynamic), sorts by material, renders efficiently.
 */
class BatchRenderer {
public:
    BatchRenderer();
    ~BatchRenderer() = default;

    BatchRenderer(const BatchRenderer&) = delete;
    BatchRenderer& operator=(const BatchRenderer&) = delete;
    BatchRenderer(BatchRenderer&& other) noexcept;
    BatchRenderer& operator=(BatchRenderer&& other) noexcept;

    void SetDevice(rhi::IDevice* device) { m_device = device; }
    void AddMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, const math::Mat4& transform);
    void Clear();
    void Flush();

    size_t GetBatchCount() const { return m_batches.size(); }

private:
    rhi::IDevice* m_device = nullptr;
    struct BatchEntry {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        math::Mat4 transform;
        size_t materialHash;
    };

    std::vector<BatchEntry> m_batches;
    bool m_sorted = false;

    void SortByMaterial();
};

} // namespace render
} // namespace eyelib
