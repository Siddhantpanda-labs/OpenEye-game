#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "eyelib/render/resources/Texture2D.h"
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <memory>

namespace eyelib {
namespace assets { class EditableMesh; }
namespace render { class Mesh; }
namespace geometry {

// Forward declaration
class MeshTopology;

/**
 * @brief Selection: represents a set of selected vertices, edges, or faces.
 * 
 * Selection is immutable once created. Use SelectionBuilder to create selections.
 * This design ensures selections are always valid and can be safely passed around.
 */
class Selection {
public:
    enum class Type {
        Vertices,  // Vertex selection
        Edges,     // Edge selection
        Faces      // Face selection
    };
    
    Selection();
    ~Selection() = default;
    
    Selection(const Selection&) = default;
    Selection& operator=(const Selection&) = default;
    Selection(Selection&&) noexcept = default;
    Selection& operator=(Selection&&) noexcept = default;
    
    /**
     * @brief Create empty selection.
     */
    static Selection Empty(Type type);
    
    /**
     * @brief Create selection from vertex IDs.
     */
    static Selection FromVertexIDs(const std::vector<uint32_t>& vertexIDs);
    
    /**
     * @brief Create selection from edge IDs.
     */
    static Selection FromEdgeIDs(const std::vector<uint32_t>& edgeIDs);
    
    /**
     * @brief Create selection from face IDs.
     */
    static Selection FromFaceIDs(const std::vector<uint32_t>& faceIDs);
    
    /**
     * @brief Get selection type.
     */
    Type GetType() const { return m_type; }
    
    /**
     * @brief Check if selection is empty.
     */
    bool IsEmpty() const { return m_ids.empty(); }
    
    /**
     * @brief Get selection count.
     */
    size_t GetCount() const { return m_ids.size(); }
    
    /**
     * @brief Check if ID is selected.
     */
    bool Contains(uint32_t id) const;
    
    /**
     * @brief Get all selected IDs.
     */
    const std::vector<uint32_t>& GetIDs() const { return m_ids; }
    
    /**
     * @brief Get selected IDs as set (for fast lookup).
     */
    const std::unordered_set<uint32_t>& GetIDSet() const { return m_idSet; }
    
    /**
     * @brief Combine two selections (union).
     */
    Selection Union(const Selection& other) const;
    
    /**
     * @brief Intersect two selections.
     */
    Selection Intersect(const Selection& other) const;
    
    /**
     * @brief Subtract other selection from this.
     */
    Selection Subtract(const Selection& other) const;
    
    /**
     * @brief Invert selection (select all except these).
     */
    Selection Invert(size_t totalCount) const;

private:
    Type m_type = Type::Vertices;
    std::vector<uint32_t> m_ids;              // Sorted list of IDs
    std::unordered_set<uint32_t> m_idSet;     // Fast lookup set
    
    Selection(Type type, const std::vector<uint32_t>& ids);
    void BuildSet();
};

/**
 * @brief SelectionBuilder: fluent API for building selections.
 * 
 * Example:
 *   auto sel = SelectionBuilder(mesh)
 *       .ByVertexIDs({0, 1, 2})
 *       .ByBounds(aabb)
 *       .Build();
 */
class SelectionBuilder {
public:
    explicit SelectionBuilder(const assets::EditableMesh& editable);
    ~SelectionBuilder() = default;
    
    SelectionBuilder(const SelectionBuilder&) = delete;
    SelectionBuilder& operator=(const SelectionBuilder&) = delete;
    
    /**
     * @brief Select by vertex IDs.
     */
    SelectionBuilder& ByVertexIDs(const std::vector<uint32_t>& vertexIDs);
    
    /**
     * @brief Select by edge IDs.
     */
    SelectionBuilder& ByEdgeIDs(const std::vector<uint32_t>& edgeIDs);
    
    /**
     * @brief Select by face IDs.
     */
    SelectionBuilder& ByFaceIDs(const std::vector<uint32_t>& faceIDs);
    
    /**
     * @brief Select vertices within bounding box.
     */
    SelectionBuilder& ByBounds(const math::AABB& bounds);
    
    /**
     * @brief Select vertices by normal direction.
     * @param direction Normal direction to match
     * @param threshold Dot product threshold (cosine of angle)
     */
    SelectionBuilder& ByNormal(const math::Vec3& direction, float threshold = 0.9f);
    
    /**
     * @brief Select vertices by curvature.
     * @param minCurvature Minimum curvature value (mean curvature magnitude)
     * @param maxCurvature Maximum curvature value
     */
    SelectionBuilder& ByCurvature(float minCurvature, float maxCurvature);
    
    /**
     * @brief Select vertices within distance from point.
     */
    SelectionBuilder& ByDistance(const math::Vec3& point, float radius);
    
    /**
     * @brief Select vertices by object ID from render buffer.
     * 
     * Uses object ID buffer to select vertices belonging to specific objects.
     * Requires object ID buffer from render pipeline.
     * 
     * @param objectIDBuffer Object ID buffer (RGBA8 texture, R channel contains object ID)
     * @param objectID Object ID to select (0-255)
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByObjectID(const render::Texture2D& objectIDBuffer, uint8_t objectID,
                                 const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                 int viewportWidth, int viewportHeight);
    
    /**
     * @brief Select vertices by depth buffer.
     * 
     * Selects vertices visible in depth buffer within depth range.
     * 
     * @param depthBuffer Depth buffer texture
     * @param minDepth Minimum depth value (0.0 = near, 1.0 = far)
     * @param maxDepth Maximum depth value
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByDepth(const render::Texture2D& depthBuffer, float minDepth, float maxDepth,
                              const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                              int viewportWidth, int viewportHeight);
    
    /**
     * @brief Select vertices by color buffer.
     * 
     * Selects vertices matching color in color buffer (useful for vision-based selection).
     * 
     * @param colorBuffer Color buffer texture
     * @param targetColor Target color to match
     * @param colorThreshold Color matching threshold (0.0 = exact match, 1.0 = any color)
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     */
    SelectionBuilder& ByColorBuffer(const render::Texture2D& colorBuffer, const math::Vec3& targetColor,
                                    float colorThreshold, const math::Mat4& viewMatrix,
                                    const math::Mat4& projectionMatrix, int viewportWidth, int viewportHeight);
    
    /**
     * @brief Combine with another selection (union).
     */
    SelectionBuilder& Union(const Selection& other);
    
    /**
     * @brief Intersect with another selection.
     */
    SelectionBuilder& Intersect(const Selection& other);
    
    /**
     * @brief Build final selection.
     */
    Selection Build() const;

private:
    const assets::EditableMesh& m_mesh;
    Selection m_current;
    
    void EnsureType(Selection::Type type);
};

} // namespace geometry
} // namespace eyelib
