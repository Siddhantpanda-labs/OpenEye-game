#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace geometry {

Selection::Selection() : m_type(Type::Vertices) {}

Selection Selection::Empty(Type type) {
    return Selection(type, {});
}

Selection Selection::FromVertexIDs(const std::vector<uint32_t>& vertexIDs) {
    return Selection(Type::Vertices, vertexIDs);
}

Selection Selection::FromEdgeIDs(const std::vector<uint32_t>& edgeIDs) {
    return Selection(Type::Edges, edgeIDs);
}

Selection Selection::FromFaceIDs(const std::vector<uint32_t>& faceIDs) {
    return Selection(Type::Faces, faceIDs);
}

Selection::Selection(Type type, const std::vector<uint32_t>& ids) 
    : m_type(type), m_ids(ids) {
    // Sort and remove duplicates
    std::sort(m_ids.begin(), m_ids.end());
    m_ids.erase(std::unique(m_ids.begin(), m_ids.end()), m_ids.end());
    BuildSet();
}

void Selection::BuildSet() {
    m_idSet.clear();
    m_idSet.insert(m_ids.begin(), m_ids.end());
}

bool Selection::Contains(uint32_t id) const {
    return m_idSet.find(id) != m_idSet.end();
}

Selection Selection::Union(const Selection& other) const {
    if (m_type != other.m_type) {
        return *this; // Can't union different types
    }
    
    std::unordered_set<uint32_t> combined;
    combined.insert(m_ids.begin(), m_ids.end());
    combined.insert(other.m_ids.begin(), other.m_ids.end());
    
    std::vector<uint32_t> result(combined.begin(), combined.end());
    return Selection(m_type, result);
}

Selection Selection::Intersect(const Selection& other) const {
    if (m_type != other.m_type) {
        return Empty(m_type);
    }
    
    std::vector<uint32_t> result;
    for (uint32_t id : m_ids) {
        if (other.Contains(id)) {
            result.push_back(id);
        }
    }
    
    return Selection(m_type, result);
}

Selection Selection::Subtract(const Selection& other) const {
    if (m_type != other.m_type) {
        return *this;
    }
    
    std::vector<uint32_t> result;
    for (uint32_t id : m_ids) {
        if (!other.Contains(id)) {
            result.push_back(id);
        }
    }
    
    return Selection(m_type, result);
}

Selection Selection::Invert(size_t totalCount) const {
    std::vector<uint32_t> result;
    for (size_t i = 0; i < totalCount; ++i) {
        if (!Contains(static_cast<uint32_t>(i))) {
            result.push_back(static_cast<uint32_t>(i));
        }
    }
    
    return Selection(m_type, result);
}

// SelectionBuilder implementation

SelectionBuilder::SelectionBuilder(const geometry::EditableMesh& editable) 
    : m_mesh(editable), m_current(Selection::Empty(Selection::Type::Vertices)) {}

SelectionBuilder& SelectionBuilder::ByVertexIDs(const std::vector<uint32_t>& vertexIDs) {
    EnsureType(Selection::Type::Vertices);
    m_current = m_current.Union(Selection::FromVertexIDs(vertexIDs));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByEdgeIDs(const std::vector<uint32_t>& edgeIDs) {
    EnsureType(Selection::Type::Edges);
    m_current = m_current.Union(Selection::FromEdgeIDs(edgeIDs));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByFaceIDs(const std::vector<uint32_t>& faceIDs) {
    EnsureType(Selection::Type::Faces);
    m_current = m_current.Union(Selection::FromFaceIDs(faceIDs));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByBounds(const math::AABB& bounds) {
    EnsureType(Selection::Type::Vertices);
    
    const auto& vertices = m_mesh.GetVertices();
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (bounds.Contains(vertices[i].position)) {
            selected.push_back(static_cast<uint32_t>(i));
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByNormal(const math::Vec3& direction, float threshold) {
    EnsureType(Selection::Type::Vertices);
    
    const auto& vertices = m_mesh.GetVertices();
    math::Vec3 normalizedDir = glm::normalize(direction);
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        math::Vec3 normal = glm::normalize(vertices[i].normal);
        float dot = glm::dot(normal, normalizedDir);
        if (dot >= threshold) {
            selected.push_back(static_cast<uint32_t>(i));
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByCurvature(float minCurvature, float maxCurvature) {
    EnsureType(Selection::Type::Vertices);
    
    const auto& vertices = m_mesh.GetVertices();
    if (vertices.empty()) return *this;
    
    // Build topology for curvature computation
    MeshTopology topology;
    const auto& indices = m_mesh.GetIndices();
    
    if (indices.empty()) return *this;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return *this;
    
    // Compute mean curvature for each vertex
    std::vector<float> curvatures(vertices.size(), 0.0f);
    
    for (size_t vid = 0; vid < vertices.size(); ++vid) {
        const auto& vertexFaces = topology.GetVertexFaces(static_cast<uint32_t>(vid));
        if (vertexFaces.empty()) continue;
        
        math::Vec3 pos = vertices[vid].position;
        math::Vec3 laplacian(0.0f);
        float totalArea = 0.0f;
        
        // Compute discrete Laplacian (mean curvature normal)
        for (uint32_t faceIdx : vertexFaces) {
            if (faceIdx >= topology.GetFaceCount()) continue;
            
            const auto& face = topology.GetFace(faceIdx);
            if (face.v0 >= vertices.size() || face.v1 >= vertices.size() || face.v2 >= vertices.size()) continue;
            
            // Find the two neighbors in this face
            uint32_t n0 = (face.v0 == vid) ? face.v1 : (face.v1 == vid) ? face.v0 : face.v2;
            uint32_t n1 = (face.v0 == vid) ? face.v2 : (face.v1 == vid) ? face.v2 : face.v0;
            
            if (n0 >= vertices.size() || n1 >= vertices.size()) continue;
            
            math::Vec3 edge0 = vertices[n0].position - pos;
            math::Vec3 edge1 = vertices[n1].position - pos;
            
            // Triangle area
            float area = 0.5f * glm::length(glm::cross(edge0, edge1));
            totalArea += area;
            
            // Contribution to Laplacian
            laplacian += (vertices[n0].position + vertices[n1].position - pos * 2.0f) * area;
        }
        
        if (totalArea > 0.001f) {
            laplacian /= totalArea;
            // Mean curvature magnitude is half the Laplacian length
            curvatures[vid] = glm::length(laplacian) * 0.5f;
        }
    }
    
    // Select vertices within curvature range
    std::vector<uint32_t> selected;
    for (size_t i = 0; i < curvatures.size(); ++i) {
        if (curvatures[i] >= minCurvature && curvatures[i] <= maxCurvature) {
            selected.push_back(static_cast<uint32_t>(i));
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByDistance(const math::Vec3& point, float radius) {
    EnsureType(Selection::Type::Vertices);
    
    const auto& vertices = m_mesh.GetVertices();
    float radiusSq = radius * radius;
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        math::Vec3 diff = vertices[i].position - point;
        float distSq = glm::dot(diff, diff);
        if (distSq <= radiusSq) {
            selected.push_back(static_cast<uint32_t>(i));
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByObjectID(const render::Texture2D& objectIDBuffer, uint8_t objectID,
                                              const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                              int viewportWidth, int viewportHeight) {
    EnsureType(Selection::Type::Vertices);
    
    if (!objectIDBuffer.IsValid() || viewportWidth <= 0 || viewportHeight <= 0) {
        return *this;
    }
    
    const auto& vertices = m_mesh.GetVertices();
    if (vertices.empty()) return *this;
    
    // Read object ID buffer
    std::vector<uint8_t> bufferData = objectIDBuffer.GetData();
    if (bufferData.empty()) return *this;
    
    math::Mat4 mvp = projectionMatrix * viewMatrix;
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        math::Vec4 clipPos = mvp * math::Vec4(vertices[i].position, 1.0f);
        
        // Perspective divide
        if (std::abs(clipPos.w) < 0.0001f) continue;
        clipPos /= clipPos.w;
        
        // Check if vertex is in view frustum
        if (clipPos.x < -1.0f || clipPos.x > 1.0f ||
            clipPos.y < -1.0f || clipPos.y > 1.0f ||
            clipPos.z < -1.0f || clipPos.z > 1.0f) {
            continue;
        }
        
        // Convert to screen coordinates
        float screenX = (clipPos.x + 1.0f) * 0.5f * viewportWidth;
        float screenY = (1.0f - clipPos.y) * 0.5f * viewportHeight; // Flip Y
        
        int px = static_cast<int>(screenX);
        int py = static_cast<int>(screenY);
        px = std::clamp(px, 0, viewportWidth - 1);
        py = std::clamp(py, 0, viewportHeight - 1);
        
        // Sample object ID from buffer
        int idx = (py * viewportWidth + px) * 4; // Assuming RGBA8
        if (idx < static_cast<int>(bufferData.size())) {
            uint8_t bufferID = bufferData[idx];
            if (bufferID == objectID) {
                selected.push_back(static_cast<uint32_t>(i));
            }
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByDepth(const render::Texture2D& depthBuffer, float minDepth, float maxDepth,
                                           const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                           int viewportWidth, int viewportHeight) {
    EnsureType(Selection::Type::Vertices);
    
    if (!depthBuffer.IsValid() || viewportWidth <= 0 || viewportHeight <= 0) {
        return *this;
    }
    
    const auto& vertices = m_mesh.GetVertices();
    if (vertices.empty()) return *this;
    
    std::vector<uint8_t> bufferData = depthBuffer.GetData();
    if (bufferData.empty()) return *this;
    
    math::Mat4 mvp = projectionMatrix * viewMatrix;
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        math::Vec4 clipPos = mvp * math::Vec4(vertices[i].position, 1.0f);
        
        if (std::abs(clipPos.w) < 0.0001f) continue;
        clipPos /= clipPos.w;
        
        // Check if vertex is in view frustum
        if (clipPos.x < -1.0f || clipPos.x > 1.0f ||
            clipPos.y < -1.0f || clipPos.y > 1.0f ||
            clipPos.z < -1.0f || clipPos.z > 1.0f) {
            continue;
        }
        
        // Get depth from clip space (z is in [-1, 1], convert to [0, 1])
        float depth = (clipPos.z + 1.0f) * 0.5f;
        
        if (depth >= minDepth && depth <= maxDepth) {
            // Also check depth buffer to ensure vertex is actually visible
            float screenX = (clipPos.x + 1.0f) * 0.5f * viewportWidth;
            float screenY = (1.0f - clipPos.y) * 0.5f * viewportHeight;
            
            int px = static_cast<int>(screenX);
            int py = static_cast<int>(screenY);
            px = std::clamp(px, 0, viewportWidth - 1);
            py = std::clamp(py, 0, viewportHeight - 1);
            
            int idx = py * viewportWidth + px;
            if (idx < static_cast<int>(bufferData.size())) {
                // Depth buffer is typically single channel (R or depth format)
                float bufferDepth = bufferData[idx] / 255.0f;
                if (std::abs(bufferDepth - depth) < 0.01f) { // Allow small tolerance
                    selected.push_back(static_cast<uint32_t>(i));
                }
            }
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::ByColorBuffer(const render::Texture2D& colorBuffer, const math::Vec3& targetColor,
                                                  float colorThreshold, const math::Mat4& viewMatrix,
                                                  const math::Mat4& projectionMatrix, int viewportWidth, int viewportHeight) {
    EnsureType(Selection::Type::Vertices);
    
    if (!colorBuffer.IsValid() || viewportWidth <= 0 || viewportHeight <= 0) {
        return *this;
    }
    
    const auto& vertices = m_mesh.GetVertices();
    if (vertices.empty()) return *this;
    
    std::vector<uint8_t> bufferData = colorBuffer.GetData();
    if (bufferData.empty()) return *this;
    
    math::Mat4 mvp = projectionMatrix * viewMatrix;
    std::vector<uint32_t> selected;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        math::Vec4 clipPos = mvp * math::Vec4(vertices[i].position, 1.0f);
        
        if (std::abs(clipPos.w) < 0.0001f) continue;
        clipPos /= clipPos.w;
        
        // Check if vertex is in view frustum
        if (clipPos.x < -1.0f || clipPos.x > 1.0f ||
            clipPos.y < -1.0f || clipPos.y > 1.0f ||
            clipPos.z < -1.0f || clipPos.z > 1.0f) {
            continue;
        }
        
        // Convert to screen coordinates
        float screenX = (clipPos.x + 1.0f) * 0.5f * viewportWidth;
        float screenY = (1.0f - clipPos.y) * 0.5f * viewportHeight;
        
        int px = static_cast<int>(screenX);
        int py = static_cast<int>(screenY);
        px = std::clamp(px, 0, viewportWidth - 1);
        py = std::clamp(py, 0, viewportHeight - 1);
        
        // Sample color from buffer
        int idx = (py * viewportWidth + px) * 4; // Assuming RGBA8
        if (idx + 2 < static_cast<int>(bufferData.size())) {
            math::Vec3 bufferColor(
                bufferData[idx] / 255.0f,
                bufferData[idx + 1] / 255.0f,
                bufferData[idx + 2] / 255.0f
            );
            
            // Check color distance
            float colorDist = glm::length(bufferColor - targetColor);
            if (colorDist <= colorThreshold) {
                selected.push_back(static_cast<uint32_t>(i));
            }
        }
    }
    
    m_current = m_current.Union(Selection::FromVertexIDs(selected));
    return *this;
}

SelectionBuilder& SelectionBuilder::Union(const Selection& other) {
    m_current = m_current.Union(other);
    return *this;
}

SelectionBuilder& SelectionBuilder::Intersect(const Selection& other) {
    m_current = m_current.Intersect(other);
    return *this;
}

Selection SelectionBuilder::Build() const {
    return m_current;
}

void SelectionBuilder::EnsureType(Selection::Type type) {
    if (m_current.IsEmpty()) {
        m_current = Selection::Empty(type);
    } else if (m_current.GetType() != type) {
        // Reset to new type if different
        m_current = Selection::Empty(type);
    }
}

} // namespace geometry
} // namespace eyelib
