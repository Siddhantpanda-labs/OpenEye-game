#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace geometry {

/**
 * @brief Edge structure: represents a connection between two vertices.
 * 
 * Edges are stored as ordered pairs (v0, v1) where v0 < v1 to ensure
 * canonical representation and enable efficient lookups.
 */
struct Edge {
    uint32_t v0 = 0;  // First vertex (always < v1)
    uint32_t v1 = 0;  // Second vertex (always > v0)
    
    Edge() = default;
    Edge(uint32_t a, uint32_t b) {
        if (a < b) {
            v0 = a;
            v1 = b;
        } else {
            v0 = b;
            v1 = a;
        }
    }
    
    bool operator==(const Edge& other) const {
        return v0 == other.v0 && v1 == other.v1;
    }
    
    bool operator<(const Edge& other) const {
        if (v0 != other.v0) return v0 < other.v0;
        return v1 < other.v1;
    }
};

// Hash function for Edge (required for unordered containers)
struct EdgeHash {
    size_t operator()(const Edge& e) const {
        return std::hash<uint64_t>()(static_cast<uint64_t>(e.v0) | (static_cast<uint64_t>(e.v1) << 32));
    }
};

/**
 * @brief Face structure: represents a triangle face.
 * 
 * Contains vertex indices and optional metadata (material group, etc.)
 */
struct Face {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    uint32_t v2 = 0;
    int32_t materialGroup = -1;  // -1 = no group
    
    Face() = default;
    Face(uint32_t a, uint32_t b, uint32_t c) : v0(a), v1(b), v2(c) {}
    
    bool operator==(const Face& other) const {
        return v0 == other.v0 && v1 == other.v1 && v2 == other.v2;
    }
    
    /**
     * @brief Get the three edges of this face.
     */
    std::vector<Edge> GetEdges() const {
        return {
            Edge(v0, v1),
            Edge(v1, v2),
            Edge(v2, v0)
        };
    }
};

/**
 * @brief Mesh topology: edge and face data structures with adjacency information.
 * 
 * This class provides efficient access to mesh topology for geometry operations.
 * It builds and maintains:
 * - Edge list and lookup
 * - Face list
 * - Vertex-to-edge adjacency
 * - Vertex-to-face adjacency
 * - Edge-to-face adjacency
 */
class MeshTopology {
public:
    MeshTopology();
    ~MeshTopology() = default;
    
    MeshTopology(const MeshTopology&) = default;
    MeshTopology& operator=(const MeshTopology&) = default;
    MeshTopology(MeshTopology&&) noexcept = default;
    MeshTopology& operator=(MeshTopology&&) noexcept = default;
    
    /**
     * @brief Build topology from vertex and index arrays.
     * 
     * @param vertices Vertex count (for validation)
     * @param indices Triangle indices (must be multiple of 3)
     */
    void BuildFromIndices(size_t vertexCount, const std::vector<uint32_t>& indices);
    
    /**
     * @brief Build topology from face list directly.
     */
    void BuildFromFaces(size_t vertexCount, const std::vector<Face>& faces);
    
    /**
     * @brief Clear all topology data.
     */
    void Clear();
    
    /**
     * @brief Check if topology is valid (has been built).
     */
    bool IsValid() const { return !m_faces.empty(); }
    
    // Edge queries
    size_t GetEdgeCount() const { return m_edges.size(); }
    const std::vector<Edge>& GetEdges() const { return m_edges; }
    bool HasEdge(const Edge& edge) const;
    bool HasEdge(uint32_t v0, uint32_t v1) const;
    
    // Face queries
    size_t GetFaceCount() const { return m_faces.size(); }
    const std::vector<Face>& GetFaces() const { return m_faces; }
    const Face& GetFace(size_t index) const { return m_faces[index]; }
    
    // Adjacency queries
    /**
     * @brief Get all edges connected to a vertex.
     */
    const std::vector<uint32_t>& GetVertexEdges(uint32_t vertexID) const;
    
    /**
     * @brief Get all faces connected to a vertex.
     */
    const std::vector<uint32_t>& GetVertexFaces(uint32_t vertexID) const;
    
    /**
     * @brief Get all faces connected to an edge.
     */
    const std::vector<uint32_t>& GetEdgeFaces(uint32_t edgeIndex) const;
    
    /**
     * @brief Find edge index by vertex pair.
     * @return Edge index or UINT32_MAX if not found.
     */
    uint32_t FindEdge(uint32_t v0, uint32_t v1) const;
    
    /**
     * @brief Check if edge is a boundary edge (only one face).
     */
    bool IsBoundaryEdge(uint32_t edgeIndex) const;
    
    /**
     * @brief Check if vertex is on boundary (has at least one boundary edge).
     */
    bool IsBoundaryVertex(uint32_t vertexID) const;
    
    /**
     * @brief Get vertex valence (number of edges connected to vertex).
     */
    uint32_t GetVertexValence(uint32_t vertexID) const;
    
    /**
     * @brief Rebuild adjacency information (call after modifying faces).
     */
    void RebuildAdjacency();

private:
    std::vector<Edge> m_edges;                              // All edges
    std::vector<Face> m_faces;                              // All faces
    std::unordered_map<Edge, uint32_t, EdgeHash> m_edgeMap; // Edge -> index lookup
    
    // Adjacency data
    std::vector<std::vector<uint32_t>> m_vertexToEdges;  // vertexID -> edge indices
    std::vector<std::vector<uint32_t>> m_vertexToFaces;   // vertexID -> face indices
    std::vector<std::vector<uint32_t>> m_edgeToFaces;     // edgeIndex -> face indices
    
    size_t m_vertexCount = 0;
    
    void BuildEdgesFromFaces();
    void BuildAdjacency();
};

} // namespace geometry
} // namespace eyelib
