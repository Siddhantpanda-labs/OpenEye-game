#pragma once

#include "Skeleton.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace assets { class EditableMesh; }

namespace animation {

/**
 * @brief Utilities for automatic rigging and bone weight calculation.
 * Supports distance-based and heat diffusion weight assignment.
 * Operates on CPU mesh (EditableMesh); caller syncs to GPU via UploadToGPU(device).
 */
class RiggingUtils {
public:
    /**
     * @brief Calculate bone weights for vertices based on distance to bone positions.
     * Uses inverse distance weighting (closer vertices get higher weights).
     * 
     * @param editable CPU mesh to assign weights to (modified in-place)
     * @param skeleton Skeleton with bones
     * @param falloffDistance Maximum distance for bone influence
     * @param power Power for distance weighting (higher = sharper falloff, default 2.0)
     */
    static void CalculateDistanceWeights(
        assets::EditableMesh& editable,
        std::shared_ptr<Skeleton> skeleton,
        float falloffDistance = 1.0f,
        float power = 2.0f);

    /**
     * @brief Assign bone weights to specific vertices based on bone positions.
     * 
     * @param vertices Vertex array to modify
     * @param skeleton Skeleton with bones
     * @param boneIndicesPerVertex Which bones to consider for each vertex (max 4 bones per vertex)
     * @param falloffDistance Maximum distance for bone influence
     * @param power Power for distance weighting
     */
    static void AssignDistanceWeights(
        std::vector<render::Vertex>& vertices,
        std::shared_ptr<Skeleton> skeleton,
        const std::vector<std::vector<size_t>>& boneIndicesPerVertex,
        float falloffDistance = 1.0f,
        float power = 2.0f);

    /**
     * @brief Calculate bone weights using heat diffusion method (smoother, more natural).
     * 
     * @param editable CPU mesh to assign weights to (modified in-place)
     * @param skeleton Skeleton with bones
     * @param iterations Number of diffusion iterations (more = smoother, default 10)
     */
    static void CalculateHeatDiffusionWeights(
        assets::EditableMesh& editable,
        std::shared_ptr<Skeleton> skeleton,
        int iterations = 10);

    /**
     * @brief Assign a single bone to vertices within a distance threshold.
     * Useful for simple rigging where each vertex belongs to one bone.
     * 
     * @param vertices Vertex array to modify
     * @param skeleton Skeleton with bones
     * @param boneIndex Index of bone to assign
     * @param maxDistance Maximum distance for assignment
     */
    static void AssignSingleBone(
        std::vector<render::Vertex>& vertices,
        std::shared_ptr<Skeleton> skeleton,
        size_t boneIndex,
        float maxDistance = 0.5f);

    /**
     * @brief Normalize bone weights so they sum to 1.0 for each vertex.
     * 
     * @param vertices Vertex array to normalize
     */
    static void NormalizeWeights(std::vector<render::Vertex>& vertices);

    /**
     * @brief Get bone world position from skeleton (bind pose).
     * 
     * @param skeleton Skeleton
     * @param boneIndex Bone index
     * @return World position of bone in bind pose
     */
    static math::Vec3 GetBoneWorldPosition(
        std::shared_ptr<Skeleton> skeleton,
        size_t boneIndex);
};

} // namespace animation
} // namespace eyelib
