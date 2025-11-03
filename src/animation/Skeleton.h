#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace OpenEye {
namespace Animation {

/**
 * @brief Represents a single bone in the skeleton hierarchy.
 */
struct Bone {
    std::string name;
    int parentIndex = -1;  // -1 if root bone
    
    // Bind pose (T-pose/A-pose transform in local space)
    glm::vec3 bindPosition = glm::vec3(0.0f);
    glm::quat bindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 bindScale = glm::vec3(1.0f);
    
    // Inverse bind matrix (transforms from bind pose to bone space)
    // Used for skinning: finalMatrix = boneMatrix * inverseBindMatrix
    glm::mat4 inverseBindMatrix = glm::mat4(1.0f);
    
    // Cached local transform matrix (bind pose)
    glm::mat4 GetBindLocalMatrix() const {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), bindPosition);
        transform = transform * glm::mat4_cast(bindRotation);
        transform = glm::scale(transform, bindScale);
        return transform;
    }
};

/**
 * @brief Complete skeleton structure with bone hierarchy.
 * 
 * Stores bone hierarchy, bind poses, and provides utilities
 * for bone lookup and hierarchy traversal.
 */
class Skeleton {
public:
    Skeleton() = default;
    ~Skeleton() = default;
    
    // Non-copyable (skeletons are unique per model)
    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;
    
    // Movable
    Skeleton(Skeleton&&) noexcept = default;
    Skeleton& operator=(Skeleton&&) noexcept = default;
    
    // Bone management
    void AddBone(const Bone& bone);
    void Clear();
    
    // Getters
    size_t GetBoneCount() const { return m_bones.size(); }
    const Bone& GetBone(size_t index) const { return m_bones[index]; }
    Bone& GetBone(size_t index) { return m_bones[index]; }
    
    // Bone lookup by name
    int FindBoneIndex(const std::string& name) const;
    const Bone* FindBone(const std::string& name) const;
    
    // Root bone index (first bone with parentIndex == -1)
    int GetRootBoneIndex() const;
    
    // Build bone name -> index mapping
    void BuildBoneMap();
    
    // Get all bones
    const std::vector<Bone>& GetBones() const { return m_bones; }
    
    // Validate skeleton (check for cycles, valid parent indices)
    bool IsValid() const;

    // Global inverse transform (from scene/model root). Needed for FBX/mixed roots
    void SetGlobalInverseTransform(const glm::mat4& m) { m_globalInverseTransform = m; }
    const glm::mat4& GetGlobalInverseTransform() const { return m_globalInverseTransform; }

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int> m_boneNameToIndex;
    bool m_boneMapBuilt = false;
    glm::mat4 m_globalInverseTransform = glm::mat4(1.0f);
};

} // namespace Animation
} // namespace OpenEye

