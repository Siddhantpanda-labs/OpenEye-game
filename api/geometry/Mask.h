#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace eyelib {
namespace render { class Mesh; }
namespace geometry { class Mask; } // Forward declaration
namespace api {
class Mesh; // Forward declaration for API Mesh
namespace geometry {

class Selection;

/**
 * @brief Blend modes for mask operations.
 */
enum class BlendMode {
    Replace,
    Add,
    Multiply,
    Subtract,
    Min,
    Max
};

/**
 * @brief Mask: weight map for vertices (public API).
 */
class Mask {
public:
    static Mask Create(const Mesh& mesh);
    static Mask CreateFilled(const Mesh& mesh, float value);
    
    Mask();
    ~Mask();
    
    Mask(const Mask&);
    Mask& operator=(const Mask&);
    Mask(Mask&&) noexcept;
    Mask& operator=(Mask&&) noexcept;
    
    void SetWeight(uint32_t vertexID, float weight);
    float GetWeight(uint32_t vertexID) const;
    bool HasWeight(uint32_t vertexID) const;
    
    void Fill(float value);
    void Fill(const Selection& selection, float value);
    void Paint(const Mesh& mesh, const Selection& selection, float value, float falloff = 0.0f);
    
    void Blend(const Mask& other, BlendMode mode);
    void Invert();
    
    size_t GetVertexCount() const;
    std::vector<float> GetWeights() const;
    void Clear();
    
    // Internal conversion (for use by other API classes)
    const ::eyelib::geometry::Mask& GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace geometry
} // namespace api
} // namespace eyelib
