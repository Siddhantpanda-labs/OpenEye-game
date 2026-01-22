#include "BoneMask.h"
#include "animation/Skeleton.h"
#include "core/log/Log.h"
#include <unordered_set>

namespace OpenEye {
namespace Animation {
namespace Controller {

CompiledBoneMask BoneMaskCompiler::Compile(const BoneMask& mask, const Skeleton& skeleton) {
    CompiledBoneMask out;
    out.name = mask.name;
    out.weights.assign(skeleton.GetBoneCount(), 1.0f);

    if (mask.includedBoneNames.empty()) {
        // full body
        return out;
    }

    // If a list is provided, start with 0 and enable listed bones (and their children).
    out.weights.assign(skeleton.GetBoneCount(), 0.0f);

    std::unordered_set<int> roots;
    for (const auto& boneName : mask.includedBoneNames) {
        int idx = skeleton.FindBoneIndex(boneName);
        if (idx >= 0) roots.insert(idx);
    }

    // Mark included roots and descendants.
    // Simple O(N^2) traversal; fine for typical bone counts.
    for (int root : roots) {
        out.weights[size_t(root)] = 1.0f;
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < skeleton.GetBoneCount(); ++i) {
                const auto& b = skeleton.GetBone(i);
                if (b.parentIndex >= 0 && out.weights[size_t(b.parentIndex)] > 0.5f && out.weights[i] < 0.5f) {
                    out.weights[i] = 1.0f;
                    changed = true;
                }
            }
        }
    }

    return out;
}

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

