#include "Skeleton.h"
#include "core/log/Log.h"

namespace OpenEye {
namespace Animation {

void Skeleton::AddBone(const Bone& bone) {
    m_bones.push_back(bone);
    m_boneMapBuilt = false;
}

void Skeleton::Clear() {
    m_bones.clear();
    m_boneNameToIndex.clear();
    m_boneMapBuilt = false;
}

int Skeleton::FindBoneIndex(const std::string& name) const {
    if (!m_boneMapBuilt) {
        // Build map on-the-fly if not built
        std::unordered_map<std::string, int> tempMap;
        for (size_t i = 0; i < m_bones.size(); ++i) {
            tempMap[m_bones[i].name] = static_cast<int>(i);
        }
        auto it = tempMap.find(name);
        return (it != tempMap.end()) ? it->second : -1;
    }
    
    auto it = m_boneNameToIndex.find(name);
    return (it != m_boneNameToIndex.end()) ? it->second : -1;
}

const Bone* Skeleton::FindBone(const std::string& name) const {
    int index = FindBoneIndex(name);
    return (index >= 0 && index < static_cast<int>(m_bones.size())) ? &m_bones[index] : nullptr;
}

int Skeleton::GetRootBoneIndex() const {
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].parentIndex == -1) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Skeleton::BuildBoneMap() {
    m_boneNameToIndex.clear();
    for (size_t i = 0; i < m_bones.size(); ++i) {
        m_boneNameToIndex[m_bones[i].name] = static_cast<int>(i);
    }
    m_boneMapBuilt = true;
}

bool Skeleton::IsValid() const {
    if (m_bones.empty()) {
        return false;
    }
    
    // Check for valid parent indices
    for (const auto& bone : m_bones) {
        if (bone.parentIndex >= static_cast<int>(m_bones.size())) {
            OE_WARN_F("Invalid parent index %d for bone '%s' (max: %zu)", 
                     bone.parentIndex, bone.name.c_str(), m_bones.size() - 1);
            return false;
        }
        
        // Check for self-parent (shouldn't happen)
        int selfIndex = static_cast<int>(&bone - &m_bones[0]);
        if (bone.parentIndex == selfIndex) {
            OE_WARN_F("Bone '%s' has itself as parent", bone.name.c_str());
            return false;
        }
    }
    
    // Check for cycles (basic check - each bone should have path to root)
    for (size_t i = 0; i < m_bones.size(); ++i) {
        int current = static_cast<int>(i);
        std::vector<bool> visited(m_bones.size(), false);
        
        while (current != -1 && current < static_cast<int>(m_bones.size())) {
            if (visited[current]) {
                OE_WARN_F("Cycle detected in bone hierarchy at bone '%s'", 
                         m_bones[current].name.c_str());
                return false;
            }
            visited[current] = true;
            current = m_bones[current].parentIndex;
        }
    }
    
    return true;
}

} // namespace Animation
} // namespace OpenEye

