#pragma once

#include <vector>

namespace eyelib {
namespace rendering {

class IRenderFeature;

/** Non-owning ordered list; lifetime of features is owned by the app / RenderWorld caller. */
class RenderFeatureRegistry {
public:
    RenderFeatureRegistry() = default;

    /** @return false if null or already registered (stable subscription order: first successful add wins). */
    bool TryAdd(IRenderFeature* feature);
    /** @return false if not found or null. Preserves relative order of remaining features. */
    bool TryRemove(IRenderFeature* feature) noexcept;
    void Clear() noexcept;

    bool Contains(const IRenderFeature* feature) const noexcept;
    size_t Count() const noexcept { return m_features.size(); }

    const std::vector<IRenderFeature*>& OrderedFeatures() const noexcept { return m_features; }

private:
    std::vector<IRenderFeature*> m_features;
};

}  // namespace rendering
}  // namespace eyelib
