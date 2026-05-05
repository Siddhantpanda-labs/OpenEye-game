#pragma once

#include "eyelib/geometry/selection/Selection.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace eyelib {
namespace assets { class EditableMesh; }
namespace render { class Mesh; }
namespace geometry {

/**
 * @brief Blend modes for mask operations.
 */
enum class BlendMode {
    Replace,   // Replace existing value
    Add,        // Add to existing value (clamped to [0,1])
    Multiply,   // Multiply with existing value
    Subtract,   // Subtract from existing value (clamped to [0,1])
    Min,        // Take minimum
    Max         // Take maximum
};

/**
 * @brief Mask: weight map for vertices (0.0 to 1.0).
 * 
 * Masks are used to control the strength of operations on specific regions.
 * A mask value of 0.0 means "no effect", 1.0 means "full effect".
 */
class Mask {
public:
    /**
     * @brief Create empty mask for a mesh (vertex count from editable).
     */
    static Mask Create(const geometry::EditableMesh& editable);
    
    /**
     * @brief Create mask with all weights set to a value.
     */
    static Mask CreateFilled(const geometry::EditableMesh& editable, float value);
    
    Mask();
    ~Mask() = default;
    
    Mask(const Mask&) = default;
    Mask& operator=(const Mask&) = default;
    Mask(Mask&&) noexcept = default;
    Mask& operator=(Mask&&) noexcept = default;
    
    /**
     * @brief Set weight for a vertex.
     * @param vertexID Vertex index
     * @param weight Weight value (will be clamped to [0,1])
     */
    void SetWeight(uint32_t vertexID, float weight);
    
    /**
     * @brief Get weight for a vertex.
     * @param vertexID Vertex index
     * @return Weight value (0.0 if vertex not in mask)
     */
    float GetWeight(uint32_t vertexID) const;
    
    /**
     * @brief Check if vertex has a weight (is in mask).
     */
    bool HasWeight(uint32_t vertexID) const;
    
    /**
     * @brief Fill all weights with a value.
     */
    void Fill(float value);
    
    /**
     * @brief Fill selection with a value.
     */
    void Fill(const Selection& selection, float value);
    
    /**
     * @brief Paint weight in a region with falloff.
     * @param editable Mesh vertices (for positions)
     * @param selection Region to paint
     * @param value Target weight value
     * @param falloff Falloff radius (0 = hard edge, >0 = smooth falloff)
     */
    void Paint(const geometry::EditableMesh& editable, const Selection& selection, float value, float falloff = 0.0f);
    
    /**
     * @brief Blend this mask with another mask.
     * @param other Other mask to blend with
     * @param mode Blend mode
     */
    void Blend(const Mask& other, BlendMode mode);
    
    /**
     * @brief Invert mask (1.0 - weight for all vertices).
     */
    void Invert();
    
    /**
     * @brief Get vertex count.
     */
    size_t GetVertexCount() const { return m_weights.size(); }
    
    /**
     * @brief Get all weights (for iteration).
     */
    const std::vector<float>& GetWeights() const { return m_weights; }
    
    /**
     * @brief Clear mask (set all weights to 0).
     */
    void Clear();

private:
    std::vector<float> m_weights;  // Weight per vertex (indexed by vertexID)
    size_t m_vertexCount = 0;
    
    Mask(size_t vertexCount);
    float ClampWeight(float weight) const;
};

} // namespace geometry
} // namespace eyelib
