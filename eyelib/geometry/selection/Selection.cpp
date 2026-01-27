#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/render/resources/Mesh.h"
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

SelectionBuilder::SelectionBuilder(const render::Mesh& mesh) 
    : m_mesh(mesh), m_current(Selection::Empty(Selection::Type::Vertices)) {}

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
