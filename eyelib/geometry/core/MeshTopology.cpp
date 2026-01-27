#include "eyelib/geometry/core/MeshTopology.h"
#include <algorithm>
#include <cassert>

namespace eyelib {
namespace geometry {

MeshTopology::MeshTopology() = default;

void MeshTopology::BuildFromIndices(size_t vertexCount, const std::vector<uint32_t>& indices) {
    Clear();
    m_vertexCount = vertexCount;
    
    if (indices.size() % 3 != 0) {
        return; // Invalid triangle data
    }
    
    m_faces.reserve(indices.size() / 3);
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t v0 = indices[i];
        uint32_t v1 = indices[i + 1];
        uint32_t v2 = indices[i + 2];
        
        // Validate vertex indices
        if (v0 >= vertexCount || v1 >= vertexCount || v2 >= vertexCount) {
            continue; // Skip invalid face
        }
        
        // Skip degenerate faces
        if (v0 == v1 || v1 == v2 || v2 == v0) {
            continue;
        }
        
        m_faces.emplace_back(v0, v1, v2);
    }
    
    BuildEdgesFromFaces();
    BuildAdjacency();
}

void MeshTopology::BuildFromFaces(size_t vertexCount, const std::vector<Face>& faces) {
    Clear();
    m_vertexCount = vertexCount;
    m_faces = faces;
    
    BuildEdgesFromFaces();
    BuildAdjacency();
}

void MeshTopology::Clear() {
    m_edges.clear();
    m_faces.clear();
    m_edgeMap.clear();
    m_vertexToEdges.clear();
    m_vertexToFaces.clear();
    m_edgeToFaces.clear();
    m_vertexCount = 0;
}

bool MeshTopology::HasEdge(const Edge& edge) const {
    return m_edgeMap.find(edge) != m_edgeMap.end();
}

bool MeshTopology::HasEdge(uint32_t v0, uint32_t v1) const {
    return HasEdge(Edge(v0, v1));
}

const std::vector<uint32_t>& MeshTopology::GetVertexEdges(uint32_t vertexID) const {
    static const std::vector<uint32_t> empty;
    if (vertexID >= m_vertexToEdges.size()) {
        return empty;
    }
    return m_vertexToEdges[vertexID];
}

const std::vector<uint32_t>& MeshTopology::GetVertexFaces(uint32_t vertexID) const {
    static const std::vector<uint32_t> empty;
    if (vertexID >= m_vertexToFaces.size()) {
        return empty;
    }
    return m_vertexToFaces[vertexID];
}

const std::vector<uint32_t>& MeshTopology::GetEdgeFaces(uint32_t edgeIndex) const {
    static const std::vector<uint32_t> empty;
    if (edgeIndex >= m_edgeToFaces.size()) {
        return empty;
    }
    return m_edgeToFaces[edgeIndex];
}

uint32_t MeshTopology::FindEdge(uint32_t v0, uint32_t v1) const {
    Edge edge(v0, v1);
    auto it = m_edgeMap.find(edge);
    if (it != m_edgeMap.end()) {
        return it->second;
    }
    return UINT32_MAX;
}

bool MeshTopology::IsBoundaryEdge(uint32_t edgeIndex) const {
    if (edgeIndex >= m_edgeToFaces.size()) {
        return false;
    }
    return m_edgeToFaces[edgeIndex].size() == 1;
}

bool MeshTopology::IsBoundaryVertex(uint32_t vertexID) const {
    const auto& edges = GetVertexEdges(vertexID);
    for (uint32_t edgeIdx : edges) {
        if (IsBoundaryEdge(edgeIdx)) {
            return true;
        }
    }
    return false;
}

uint32_t MeshTopology::GetVertexValence(uint32_t vertexID) const {
    return static_cast<uint32_t>(GetVertexEdges(vertexID).size());
}

void MeshTopology::RebuildAdjacency() {
    BuildAdjacency();
}

void MeshTopology::BuildEdgesFromFaces() {
    m_edges.clear();
    m_edgeMap.clear();
    
    std::unordered_set<Edge, EdgeHash> edgeSet;
    
    for (const auto& face : m_faces) {
        Edge e0(face.v0, face.v1);
        Edge e1(face.v1, face.v2);
        Edge e2(face.v2, face.v0);
        
        edgeSet.insert(e0);
        edgeSet.insert(e1);
        edgeSet.insert(e2);
    }
    
    m_edges.assign(edgeSet.begin(), edgeSet.end());
    std::sort(m_edges.begin(), m_edges.end());
    
    // Build edge map
    for (size_t i = 0; i < m_edges.size(); ++i) {
        m_edgeMap[m_edges[i]] = static_cast<uint32_t>(i);
    }
}

void MeshTopology::BuildAdjacency() {
    // Resize adjacency arrays
    m_vertexToEdges.clear();
    m_vertexToEdges.resize(m_vertexCount);
    m_vertexToFaces.clear();
    m_vertexToFaces.resize(m_vertexCount);
    m_edgeToFaces.clear();
    m_edgeToFaces.resize(m_edges.size());
    
    // Build vertex-to-face adjacency
    for (size_t faceIdx = 0; faceIdx < m_faces.size(); ++faceIdx) {
        const Face& face = m_faces[faceIdx];
        
        if (face.v0 < m_vertexCount) m_vertexToFaces[face.v0].push_back(static_cast<uint32_t>(faceIdx));
        if (face.v1 < m_vertexCount) m_vertexToFaces[face.v1].push_back(static_cast<uint32_t>(faceIdx));
        if (face.v2 < m_vertexCount) m_vertexToFaces[face.v2].push_back(static_cast<uint32_t>(faceIdx));
    }
    
    // Build edge-to-face and vertex-to-edge adjacency
    for (size_t faceIdx = 0; faceIdx < m_faces.size(); ++faceIdx) {
        const Face& face = m_faces[faceIdx];
        auto edges = face.GetEdges();
        
        for (const Edge& edge : edges) {
            uint32_t edgeIdx = FindEdge(edge.v0, edge.v1);
            if (edgeIdx != UINT32_MAX) {
                m_edgeToFaces[edgeIdx].push_back(static_cast<uint32_t>(faceIdx));
                
                // Add to vertex-to-edge adjacency
                if (edge.v0 < m_vertexCount) {
                    auto& vEdges = m_vertexToEdges[edge.v0];
                    if (std::find(vEdges.begin(), vEdges.end(), edgeIdx) == vEdges.end()) {
                        vEdges.push_back(edgeIdx);
                    }
                }
                if (edge.v1 < m_vertexCount) {
                    auto& vEdges = m_vertexToEdges[edge.v1];
                    if (std::find(vEdges.begin(), vEdges.end(), edgeIdx) == vEdges.end()) {
                        vEdges.push_back(edgeIdx);
                    }
                }
            }
        }
    }
}

} // namespace geometry
} // namespace eyelib
