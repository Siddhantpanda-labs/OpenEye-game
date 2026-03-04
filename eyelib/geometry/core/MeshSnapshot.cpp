#include "eyelib/geometry/core/MeshSnapshot.h"
#include "eyelib/assets/EditableMesh.h"
#include "eyelib/render/resources/Mesh.h"
#include <algorithm>

namespace eyelib {
namespace geometry {

std::shared_ptr<MeshSnapshot> MeshSnapshot::Create(const assets::EditableMesh& editable) {
    auto snapshot = std::make_shared<MeshSnapshot>();
    snapshot->m_vertices = editable.GetVertices();
    snapshot->m_indices = editable.GetIndices();
    snapshot->m_submeshes = editable.GetSubmeshes();
    return snapshot;
}

void MeshSnapshot::Restore(assets::EditableMesh& editable) const {
    editable.SetVertices(m_vertices);
    editable.SetIndices(m_indices);
    editable.SetSubmeshes(m_submeshes);
}

// MeshHistory implementation

MeshHistory::MeshHistory() : m_currentIndex(0), m_maxHistorySize(100) {}

void MeshHistory::Save(const assets::EditableMesh& editable) {
    if (m_currentIndex < m_history.size()) {
        m_history.erase(m_history.begin() + m_currentIndex, m_history.end());
    }
    m_history.push_back(MeshSnapshot::Create(editable));
    m_currentIndex = m_history.size() - 1;
    TrimHistory();
}

bool MeshHistory::Undo(assets::EditableMesh& editable) {
    if (!CanUndo()) return false;
    m_currentIndex--;
    m_history[m_currentIndex]->Restore(editable);
    return true;
}

bool MeshHistory::Redo(assets::EditableMesh& editable) {
    if (!CanRedo()) return false;
    m_currentIndex++;
    m_history[m_currentIndex]->Restore(editable);
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
