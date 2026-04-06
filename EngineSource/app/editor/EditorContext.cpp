#include "EditorContext.h"
#include "GameRuntime.h"

namespace openeye::editor {

EditorContext::EditorContext() = default;

EditorContext::~EditorContext() = default;

void EditorContext::EnqueueGpuSafeWork(std::function<void()>&& fn) {
    if (fn)
        m_pendingGpuSafeWork.push_back(std::move(fn));
}

void EditorContext::DrainGpuSafeWork() {
    if (m_pendingGpuSafeWork.empty()) return;
    for (auto& fn : m_pendingGpuSafeWork) {
        if (fn)
            fn();
    }
    m_pendingGpuSafeWork.clear();
}

} // namespace openeye::editor
