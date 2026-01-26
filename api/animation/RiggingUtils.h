#pragma once

#include "Skeleton.h"
#include "../render/Mesh.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace api {

/**
 * @brief Utilities for automatic rigging and bone weight calculation.
 * Supports distance-based and heat diffusion weight assignment.
 */
class RiggingUtils {
public:
    /**
     * @brief Calculate bone weights for vertices based on distance to bone positions.
     * Uses inverse distance weighting (closer vertices get higher weights).
     * 
     * @param mesh Mesh to assign weights to
     * @param skeleton Skeleton with bones
     * @param boneIndices Indices of bones to consider for each vertex (max 4)
     * @param falloffDistance Maximum distance for bone influence
     * @param power Power for distance weighting (higher = sharper falloff, default 2.0)
     */
    static void CalculateDistanceWeights(
        std::shared_ptr<Mesh> mesh,
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
        std::vector<Mesh::Vertex>& vertices,
        std::shared_ptr<Skeleton> skeleton,
        const std::vector<std::vector<size_t>>& boneIndicesPerVertex,
        float falloffDistance = 1.0f,
        float power = 2.0f);

    /**
     * @brief Calculate bone weights using heat diffusion method (smoother, more natural).
     * 
     * @param mesh Mesh to assign weights to
     * @param skeleton Skeleton with bones
     * @param iterations Number of diffusion iterations (more = smoother, default 10)
     */
    static void CalculateHeatDiffusionWeights(
        std::shared_ptr<Mesh> mesh,
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
        std::vector<Mesh::Vertex>& vertices,
        std::shared_ptr<Skeleton> skeleton,
        size_t boneIndex,
        float maxDistance = 0.5f);

    /**
     * @brief Normalize bone weights so they sum to 1.0 for each vertex.
     * 
     * @param vertices Vertex array to normalize
     */
    static void NormalizeWeights(std::vector<Mesh::Vertex>& vertices);

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

} // namespace api
} // namespace eyelib
