#include "eyelib/animation/RiggingUtils.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace eyelib {
namespace animation {

void RiggingUtils::CalculateDistanceWeights(
    std::shared_ptr<render::Mesh> mesh,
    std::shared_ptr<Skeleton> skeleton,
    float falloffDistance,
    float power) {
    
    if (!mesh || !skeleton) return;
    
    auto& vertices = mesh->GetVertices();
    if (vertices.empty()) return;
    
    // For each vertex, find closest bones and assign weights
    for (auto& vertex : vertices) {
        std::vector<std::pair<float, size_t>> boneDistances; // (distance, boneIndex)
        
        // Calculate distance to each bone
        for (size_t i = 0; i < skeleton->BoneCount(); ++i) {
            math::Vec3 bonePos = GetBoneWorldPosition(skeleton, i);
            float dist = glm::length(vertex.position - bonePos);
            
            if (dist <= falloffDistance) {
                boneDistances.push_back({dist, i});
            }
        }
        
        // Sort by distance (closest first)
        std::sort(boneDistances.begin(), boneDistances.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Take up to 4 closest bones
        size_t count = std::min(boneDistances.size(), size_t(4));
        
        // Calculate weights using inverse distance weighting
        float totalWeight = 0.0f;
        math::IVec4 indices(0, 0, 0, 0);
        math::Vec4 weights(0.0f, 0.0f, 0.0f, 0.0f);
        
        for (size_t i = 0; i < count; ++i) {
            float dist = boneDistances[i].first;
            if (dist < 0.0001f) dist = 0.0001f; // Avoid division by zero
            float weight = 1.0f / std::pow(dist, power);
            totalWeight += weight;
            indices[i] = static_cast<int>(boneDistances[i].second);
            weights[i] = weight;
        }
        
        // Normalize weights
        if (totalWeight > 0.0f) {
            weights = weights / totalWeight;
        } else {
            // No bones in range, assign to bone 0
            indices = math::IVec4(0, 0, 0, 0);
            weights = math::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
        
        vertex.boneIndices = indices;
        vertex.boneWeights = weights;
    }
    
    mesh->InvalidateAABB();
    mesh->UploadToGPU();
}

void RiggingUtils::AssignDistanceWeights(
    std::vector<render::Vertex>& vertices,
    std::shared_ptr<Skeleton> skeleton,
    const std::vector<std::vector<size_t>>& boneIndicesPerVertex,
    float falloffDistance,
    float power) {
    
    if (!skeleton || vertices.empty()) return;
    
    for (size_t vIdx = 0; vIdx < vertices.size() && vIdx < boneIndicesPerVertex.size(); ++vIdx) {
        auto& vertex = vertices[vIdx];
        const auto& candidateBones = boneIndicesPerVertex[vIdx];
        
        std::vector<std::pair<float, size_t>> boneDistances;
        
        for (size_t boneIdx : candidateBones) {
            if (boneIdx >= skeleton->BoneCount()) continue;
            math::Vec3 bonePos = GetBoneWorldPosition(skeleton, boneIdx);
            float dist = glm::length(vertex.position - bonePos);
            
            if (dist <= falloffDistance) {
                boneDistances.push_back({dist, boneIdx});
            }
        }
        
        std::sort(boneDistances.begin(), boneDistances.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        size_t count = std::min(boneDistances.size(), size_t(4));
        float totalWeight = 0.0f;
        math::IVec4 indices(0, 0, 0, 0);
        math::Vec4 weights(0.0f, 0.0f, 0.0f, 0.0f);
        
        for (size_t i = 0; i < count; ++i) {
            float dist = boneDistances[i].first;
            if (dist < 0.0001f) dist = 0.0001f;
            float weight = 1.0f / std::pow(dist, power);
            totalWeight += weight;
            indices[i] = static_cast<int>(boneDistances[i].second);
            weights[i] = weight;
        }
        
        if (totalWeight > 0.0f) {
            weights = weights / totalWeight;
        } else {
            indices = math::IVec4(0, 0, 0, 0);
            weights = math::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
        
        vertex.boneIndices = indices;
        vertex.boneWeights = weights;
    }
}

void RiggingUtils::CalculateHeatDiffusionWeights(
    std::shared_ptr<render::Mesh> mesh,
    std::shared_ptr<Skeleton> skeleton,
    int iterations) {
    
    // Simplified heat diffusion: iterative smoothing of distance-based weights
    // Full implementation would require mesh connectivity, so we use distance-based as base
    CalculateDistanceWeights(mesh, skeleton, 1.0f, 2.0f);
    
    // For now, heat diffusion is approximated by distance-based with smoothing
    // A full implementation would require mesh topology (edges/faces)
    // This is a placeholder that can be enhanced later
}

void RiggingUtils::AssignSingleBone(
    std::vector<render::Vertex>& vertices,
    std::shared_ptr<Skeleton> skeleton,
    size_t boneIndex,
    float maxDistance) {
    
    if (!skeleton || boneIndex >= skeleton->BoneCount()) return;
    
    math::Vec3 bonePos = GetBoneWorldPosition(skeleton, boneIndex);
    
    for (auto& vertex : vertices) {
        float dist = glm::length(vertex.position - bonePos);
        
        if (dist <= maxDistance) {
            vertex.boneIndices = math::IVec4(static_cast<int>(boneIndex), 0, 0, 0);
            vertex.boneWeights = math::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

void RiggingUtils::NormalizeWeights(std::vector<render::Vertex>& vertices) {
    for (auto& vertex : vertices) {
        float sum = vertex.boneWeights.x + vertex.boneWeights.y + 
                    vertex.boneWeights.z + vertex.boneWeights.w;
        
        if (sum > 0.0001f) {
            vertex.boneWeights = vertex.boneWeights / sum;
        } else {
            // No weights, assign to first bone
            vertex.boneWeights = math::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

math::Vec3 RiggingUtils::GetBoneWorldPosition(
    std::shared_ptr<Skeleton> skeleton,
    size_t boneIndex) {
    
    if (!skeleton || boneIndex >= skeleton->BoneCount()) {
        return math::Vec3(0.0f);
    }
    
    const Bone& bone = skeleton->GetBone(boneIndex);
    
    // Traverse parent chain to compute world position
    math::Vec3 worldPos = bone.bindPosition;
    int parentIdx = bone.parentIndex;
    
    while (parentIdx >= 0 && static_cast<size_t>(parentIdx) < skeleton->BoneCount()) {
        const Bone& parent = skeleton->GetBone(static_cast<size_t>(parentIdx));
        math::Mat4 parentTransform = math::Compose(parent.bindPosition, parent.bindRotation, parent.bindScale);
        worldPos = math::Vec3(parentTransform * math::Vec4(worldPos, 1.0f));
        parentIdx = parent.parentIndex;
    }
    
    return worldPos;
}

} // namespace animation
} // namespace eyelib
