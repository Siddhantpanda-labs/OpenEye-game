#pragma once

#include "eyelib/math/Types.h"
#include "api/math/AABB.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace eyelib {
namespace render { class Mesh; class Texture2D; }
namespace geometry { class Selection; } // Forward declaration
namespace api {
class Mesh; // Forward declaration for API Mesh
class Texture; // Forward declaration for API Texture
namespace geometry {

/**
 * @brief Selection: public API for selecting mesh elements.
 * 
 * Selection is immutable and represents a set of selected vertices, edges, or faces.
 * Use SelectionBuilder to create selections.
 */
class Selection {
public:
    enum class Type {
        Vertices,
        Edges,
        Faces
    };
    
    Selection();
    ~Selection();
    
    Selection(const Selection&);
    Selection& operator=(const Selection&);
    Selection(Selection&&) noexcept;
    Selection& operator=(Selection&&) noexcept;
    
    /**
     * @brief Create an empty selection of the specified type.
     */
    static Selection Empty(Type type);
    
    Type GetType() const;
    bool IsEmpty() const;
    size_t GetCount() const;
    bool Contains(uint32_t id) const;
    std::vector<uint32_t> GetIDs() const;
    
    Selection Union(const Selection& other) const;
    Selection Intersect(const Selection& other) const;
    Selection Subtract(const Selection& other) const;
    
    // Internal conversion (for use by other API classes)
    const ::eyelib::geometry::Selection& GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    friend class SelectionBuilder;
    Selection(std::unique_ptr<Impl> impl);
};

/**
 * @brief SelectionBuilder: fluent API for building selections.
 */
class SelectionBuilder {
public:
    explicit SelectionBuilder(const Mesh& mesh);
    ~SelectionBuilder();
    
    SelectionBuilder(const SelectionBuilder&) = delete;
    SelectionBuilder& operator=(const SelectionBuilder&) = delete;
    
    SelectionBuilder& ByVertexIDs(const std::vector<uint32_t>& vertexIDs);
    SelectionBuilder& ByEdgeIDs(const std::vector<uint32_t>& edgeIDs);
    SelectionBuilder& ByFaceIDs(const std::vector<uint32_t>& faceIDs);
    SelectionBuilder& ByBounds(const ::eyelib::math::AABB& bounds);
    SelectionBuilder& ByNormal(const ::eyelib::math::Vec3& direction, float threshold = 0.9f);
    SelectionBuilder& ByCurvature(float minCurvature, float maxCurvature);
    SelectionBuilder& ByDistance(const ::eyelib::math::Vec3& point, float radius);
    
    /**
     * @brief Select vertices by object ID from render buffer (vision-based).
     * 
     * @param objectIDBuffer Object ID buffer texture
     * @param objectID Object ID to select (0-255)
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByObjectID(const api::Texture& objectIDBuffer, uint8_t objectID,
                                 const ::eyelib::math::Mat4& viewMatrix, const ::eyelib::math::Mat4& projectionMatrix,
                                 int viewportWidth, int viewportHeight);
    
    /**
     * @brief Select vertices by depth buffer (vision-based).
     * 
     * @param depthBuffer Depth buffer texture
     * @param minDepth Minimum depth value (0.0 = near, 1.0 = far)
     * @param maxDepth Maximum depth value
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByDepth(const api::Texture& depthBuffer, float minDepth, float maxDepth,
                             const ::eyelib::math::Mat4& viewMatrix, const ::eyelib::math::Mat4& projectionMatrix,
                             int viewportWidth, int viewportHeight);
    
    /**
     * @brief Select vertices by color buffer (vision-based).
     * 
     * @param colorBuffer Color buffer texture
     * @param targetColor Target color to match
     * @param colorThreshold Color matching threshold (0.0 = exact match, 1.0 = any color)
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByColorBuffer(const api::Texture& colorBuffer, const ::eyelib::math::Vec3& targetColor,
                                   float colorThreshold, const ::eyelib::math::Mat4& viewMatrix,
                                   const ::eyelib::math::Mat4& projectionMatrix, int viewportWidth, int viewportHeight);
    
    SelectionBuilder& Union(const Selection& other);
    SelectionBuilder& Intersect(const Selection& other);
    
    Selection Build() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace geometry
} // namespace api
} // namespace eyelib
