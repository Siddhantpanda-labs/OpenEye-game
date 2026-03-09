#include "eyelib/animation/Skeleton.h"

#include "eyelib/core/Log.h"

namespace eyelib {
namespace animation {

void Skeleton::AddBone(const Bone& bone) {
    m_bones.push_back(bone);
    m_boneMapBuilt = false;
}

void Skeleton::ModifyBone(size_t index, const Bone& bone) {
    if (index >= m_bones.size()) return;
    m_bones[index] = bone;
    m_boneMapBuilt = false; // Rebuild map in case name changed
}

void Skeleton::RemoveBone(size_t index) {
    if (index >= m_bones.size()) return;
    
    // Remove the bone
    m_bones.erase(m_bones.begin() + static_cast<ptrdiff_t>(index));
    
    // Update parent indices for remaining bones
    for (auto& bone : m_bones) {
        if (bone.parentIndex == static_cast<int>(index)) {
            // Orphan the bone (make it root)
            bone.parentIndex = -1;
        } else if (bone.parentIndex > static_cast<int>(index)) {
            // Adjust parent index down by 1
            bone.parentIndex--;
        }
    }
    
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

