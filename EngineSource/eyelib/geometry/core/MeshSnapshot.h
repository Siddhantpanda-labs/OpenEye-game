#pragma once

#include "eyelib/render/resources/Mesh.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace eyelib {
namespace geometry {
class EditableMesh;

/**
 * @brief MeshSnapshot: Immutable snapshot of mesh state (CPU geometry).
 * 
 * Provides copy-on-write semantics for mesh operations.
 * Enables undo/redo and safe experimentation.
 */
class MeshSnapshot {
public:
    /**
     * @brief Create snapshot from editable mesh.
     */
    static std::shared_ptr<MeshSnapshot> Create(const geometry::EditableMesh& editable);
    
    /**
     * @brief Restore editable mesh from snapshot.
     */
    void Restore(geometry::EditableMesh& editable) const;
    
    /**
     * @brief Get vertex count.
     */
    size_t GetVertexCount() const { return m_vertices.size(); }
    
    /**
     * @brief Get index count.
     */
    size_t GetIndexCount() const { return m_indices.size(); }
    
    /**
     * @brief Check if snapshot is valid.
     */
    bool IsValid() const { return !m_vertices.empty(); }

    // Constructor (public for std::make_shared, but use Create() factory method)
    MeshSnapshot() = default;
    
private:
    std::vector<render::Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<render::Submesh> m_submeshes;
};

/**
 * @brief MeshHistory: Operation history for undo/redo.
 * 
 * Maintains a stack of mesh snapshots for operation history.
 */
class MeshHistory {
public:
    MeshHistory();
    ~MeshHistory() = default;
    
    /**
     * @brief Save current mesh state.
     */
    void Save(const geometry::EditableMesh& editable);
    
    /**
     * @brief Undo last operation.
     * @return true if undo was successful
     */
    bool Undo(geometry::EditableMesh& editable);
    
    /**
     * @brief Redo last undone operation.
     * @return true if redo was successful
     */
    bool Redo(geometry::EditableMesh& editable);
    
    /**
     * @brief Check if undo is available.
     */
    bool CanUndo() const { return m_currentIndex > 0; }
    
    /**
     * @brief Check if redo is available.
     */
    bool CanRedo() const { return m_currentIndex < m_history.size() - 1; }
    
    /**
     * @brief Clear history.
     */
    void Clear();
    
    /**
     * @brief Set maximum history size (to limit memory usage).
     */
    void SetMaxHistorySize(size_t maxSize) { m_maxHistorySize = maxSize; }
    
    /**
     * @brief Get current history size.
     */
    size_t GetHistorySize() const { return m_history.size(); }

private:
    std::vector<std::shared_ptr<MeshSnapshot>> m_history;
    size_t m_currentIndex = 0;
    size_t m_maxHistorySize = 100; // Default: keep last 100 operations
    
    void TrimHistory();
};

} // namespace geometry
} // namespace eyelib
