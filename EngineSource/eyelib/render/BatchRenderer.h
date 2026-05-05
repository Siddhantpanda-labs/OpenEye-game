#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class ShaderCache;

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
    /** Optional: when set, material UBO is bound for Forward/Forward+ shaders. */
    void SetCommandList(rhi::ICommandList* cmd) { m_cmd = cmd; }
    /** Required for draw: shader variant from cache. */
    void SetShaderCache(ShaderCache* cache, PipelineStage stage);
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
    ShaderCache* m_shaderCache = nullptr;
    PipelineStage m_pipelineStage = PipelineStage::Forward;
    rhi::ICommandList* m_cmd = nullptr;

    void SortByMaterial();
};

} // namespace render
} // namespace eyelib
