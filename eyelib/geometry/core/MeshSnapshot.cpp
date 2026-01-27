#include "eyelib/geometry/core/MeshSnapshot.h"
#include "eyelib/render/resources/Mesh.h"
#include <algorithm>

namespace eyelib {
namespace geometry {

std::shared_ptr<MeshSnapshot> MeshSnapshot::Create(const render::Mesh& mesh) {
    auto snapshot = std::make_shared<MeshSnapshot>();
    
    // Copy vertices
    snapshot->m_vertices = mesh.GetVertices();
    
    // Copy indices
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        snapshot->m_indices = mesh.GetIndices32();
        snapshot->m_indexFormat = render::IndexFormat::UInt32;
    } else {
        const auto& indices16 = mesh.GetIndices16();
        snapshot->m_indices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            snapshot->m_indices.push_back(static_cast<uint32_t>(idx));
        }
        snapshot->m_indexFormat = render::IndexFormat::UInt16;
    }
    
    // Copy submeshes
    snapshot->m_submeshes.clear();
    for (size_t i = 0; i < mesh.GetSubmeshCount(); ++i) {
        snapshot->m_submeshes.push_back(mesh.GetSubmesh(i));
    }
    
    return snapshot;
}

void MeshSnapshot::Restore(render::Mesh& mesh) const {
    // Restore vertices
    mesh.SetVertices(m_vertices);
    
    // Restore indices
    if (m_indexFormat == render::IndexFormat::UInt32) {
        mesh.SetIndices(m_indices);
    } else {
        std::vector<uint16_t> indices16;
        indices16.reserve(m_indices.size());
        for (uint32_t idx : m_indices) {
            if (idx > 65535) {
                // Can't fit in uint16_t, skip or clamp
                continue;
            }
            indices16.push_back(static_cast<uint16_t>(idx));
        }
        mesh.SetIndices(indices16);
    }
    
    // Restore submeshes
    mesh.ClearSubmeshes();
    for (const auto& submesh : m_submeshes) {
        mesh.AddSubmesh(submesh.indexOffset, submesh.indexCount, submesh.materialIndex);
    }
    
    // Upload to GPU
    mesh.UploadToGPU();
}

// MeshHistory implementation

MeshHistory::MeshHistory() : m_currentIndex(0), m_maxHistorySize(100) {}

void MeshHistory::Save(const render::Mesh& mesh) {
    // Remove any "future" history if we're not at the end
    if (m_currentIndex < m_history.size()) {
        m_history.erase(m_history.begin() + m_currentIndex, m_history.end());
    }
    
    // Add new snapshot
    m_history.push_back(MeshSnapshot::Create(mesh));
    m_currentIndex = m_history.size() - 1;
    
    // Trim if needed
    TrimHistory();
}

bool MeshHistory::Undo(render::Mesh& mesh) {
    if (!CanUndo()) {
        return false;
    }
    
    m_currentIndex--;
    m_history[m_currentIndex]->Restore(mesh);
    return true;
}

bool MeshHistory::Redo(render::Mesh& mesh) {
    if (!CanRedo()) {
        return false;
    }
    
    m_currentIndex++;
    m_history[m_currentIndex]->Restore(mesh);
    return true;
}

void MeshHistory::Clear() {
    m_history.clear();
    m_currentIndex = 0;
}

void MeshHistory::TrimHistory() {
    // Keep only the last m_maxHistorySize snapshots
    if (m_history.size() > m_maxHistorySize) {
        size_t removeCount = m_history.size() - m_maxHistorySize;
        m_history.erase(m_history.begin(), m_history.begin() + removeCount);
        if (m_currentIndex >= removeCount) {
            m_currentIndex -= removeCount;
        } else {
            m_currentIndex = 0;
        }
    }
}

} // namespace geometry
} // namespace eyelib
