#include "eyelib/animation/Skeleton.h"

#include "eyelib/core/Log.h"

namespace eyelib {
namespace animation {

void Skeleton::AddBone(const Bone& bone) {
    m_bones.push_back(bone);
    m_boneMapBuilt = false;
}

void Skeleton::Clear() {
    m_bones.clear();
    m_boneNameToIndex.clear();
    m_boneMapBuilt = false;
}

void Skeleton::BuildBoneMap() {
    m_boneNameToIndex.clear();
    for (int i = 0; i < static_cast<int>(m_bones.size()); ++i) {
        m_boneNameToIndex[m_bones[i].name] = i;
    }
    m_boneMapBuilt = true;
}

int Skeleton::FindBoneIndex(const std::string& name) const {
    if (!m_boneMapBuilt) {
        // best-effort linear lookup if map not built yet
        for (int i = 0; i < static_cast<int>(m_bones.size()); ++i) {
            if (m_bones[i].name == name) return i;
        }
        return -1;
    }
    auto it = m_boneNameToIndex.find(name);
    return (it != m_boneNameToIndex.end()) ? it->second : -1;
}

bool Skeleton::IsValid() const {
    // Validate parent indices and detect trivial cycles (basic)
    const int n = static_cast<int>(m_bones.size());
    for (int i = 0; i < n; ++i) {
        const int p = m_bones[i].parentIndex;
        if (p >= n) return false;
        if (p == i) return false;
    }
    return true;
}

} // namespace animation
} // namespace eyelib

