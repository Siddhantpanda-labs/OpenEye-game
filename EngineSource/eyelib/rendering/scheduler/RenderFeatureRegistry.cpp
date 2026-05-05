#include "eyelib/rendering/scheduler/RenderFeatureRegistry.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"

namespace eyelib {
namespace rendering {

bool RenderFeatureRegistry::TryAdd(IRenderFeature* feature) {
    if (!feature)
        return false;
    for (IRenderFeature* f : m_features) {
        if (f == feature)
            return false;
    }
    m_features.push_back(feature);
    return true;
}

bool RenderFeatureRegistry::TryRemove(IRenderFeature* feature) noexcept {
    if (!feature)
        return false;
    for (auto it = m_features.begin(); it != m_features.end(); ++it) {
        if (*it == feature) {
            m_features.erase(it);
            return true;
        }
    }
    return false;
}

bool RenderFeatureRegistry::Contains(const IRenderFeature* feature) const noexcept {
    if (!feature)
        return false;
    for (IRenderFeature* f : m_features) {
        if (f == feature)
            return true;
    }
    return false;
}

void RenderFeatureRegistry::Clear() noexcept {
    m_features.clear();
}

}  // namespace rendering
}  // namespace eyelib
