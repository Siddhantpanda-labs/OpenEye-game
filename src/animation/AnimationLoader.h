#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Skeleton.h"
#include "AnimationClip.h"

// Forward declarations (avoid forcing Assimp headers on app targets)
struct aiScene;
struct aiMesh;
struct aiNode;

namespace OpenEye {
namespace Animation {

/**
 * @brief Loader for extracting skeleton and animation data from Assimp scenes.
 * 
 * Extracts bone hierarchy, bind poses, vertex bone weights, and animation clips
 * from Assimp-imported model files (glTF, FBX, etc.).
 */
class AnimationLoader {
public:
    /**
     * @brief Extract skeleton from Assimp scene.
     * @param scene Assimp scene containing bone data
     * @param meshIndex Index of mesh to extract skeleton from (usually 0 for single mesh)
     * @return Shared pointer to extracted skeleton, or nullptr on failure
     */
    static std::shared_ptr<Skeleton> ExtractSkeleton(const aiScene* scene, size_t meshIndex = 0);
    
    /**
     * @brief Extract bone weights and indices for vertices.
     * @param mesh Assimp mesh containing bone weight data
     * @param skeleton Skeleton to map bone names to indices
     * @param outBoneIndices Output vector of bone indices per vertex (4 per vertex)
     * @param outBoneWeights Output vector of bone weights per vertex (4 per vertex)
     */
    static void ExtractBoneWeights(const aiMesh* mesh, 
                                   const Skeleton& skeleton,
                                   std::vector<glm::ivec4>& outBoneIndices,
                                   std::vector<glm::vec4>& outBoneWeights);
    
    /**
     * @brief Extract all animation clips from Assimp scene.
     * @param scene Assimp scene containing animations
     * @param skeleton Skeleton to map bone names to indices
     * @return Vector of animation clips
     */
    static std::vector<std::shared_ptr<AnimationClip>> ExtractAnimations(
        const aiScene* scene, 
        const Skeleton& skeleton);

    /**
     * @brief Load animation clips from a file and map them onto an existing skeleton (by bone names).
     *        This is how we can combine separate FBX files (Idle/Walk/Run) into one runtime clip set.
     */
    static std::vector<std::shared_ptr<AnimationClip>> LoadClipsForSkeletonFromFile(
        const std::string& filepath,
        const Skeleton& skeleton);

private:
    // Helper: Recursively process node hierarchy to build skeleton
    static void ProcessBoneHierarchy(aiNode* node, 
                                    const aiScene* scene,
                                    Skeleton& skeleton,
                                    std::unordered_map<std::string, int>& nodeToBoneIndex,
                                    int parentBoneIndex = -1);
    
    // Helper: Extract bone from Assimp mesh
    static void ExtractBonesFromMesh(const aiMesh* mesh,
                                     const aiScene* scene,
                                     Skeleton& skeleton,
                                     std::unordered_map<std::string, int>& nodeToBoneIndex);
    
    // NOTE: Assimp conversion helpers live in AnimationLoader.cpp on purpose,
    // to avoid pulling Assimp typedef-heavy headers into this public header.
};

} // namespace Animation
} // namespace OpenEye

