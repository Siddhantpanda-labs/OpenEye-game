#include "RiggingUtils.h"
#include "Skeleton.h"
#include "../render/Mesh.h"
#include "eyelib/animation/RiggingUtils.h"

namespace eyelib {
namespace api {

void RiggingUtils::CalculateDistanceWeights(
    std::shared_ptr<Mesh> mesh,
    std::shared_ptr<Skeleton> skeleton,
    float falloffDistance,
    float power) {
    
    if (!mesh || !skeleton) return;
    
    auto internalMesh = mesh->GetInternal();
    auto internalSkeleton = skeleton->GetInternal();
    if (!internalMesh || !internalSkeleton) return;
    
    animation::RiggingUtils::CalculateDistanceWeights(
        internalMesh, internalSkeleton, falloffDistance, power);
}

void RiggingUtils::AssignDistanceWeights(
    std::vector<Mesh::Vertex>& vertices,
    std::shared_ptr<Skeleton> skeleton,
    const std::vector<std::vector<size_t>>& boneIndicesPerVertex,
    float falloffDistance,
    float power) {
    
    if (!skeleton) return;
    
    // Convert API vertices to internal vertices
    std::vector<::eyelib::render::Vertex> internalVerts;
    internalVerts.reserve(vertices.size());
    for (const auto& v : vertices) {
        ::eyelib::render::Vertex iv;
        iv.position = v.position;
        iv.normal = v.normal;
        iv.uv = v.uv;
        iv.uv1 = v.uv1;
        iv.tangent = v.tangent;
        iv.color = v.color;
        iv.boneIndices = v.boneIndices;
        iv.boneWeights = v.boneWeights;
        internalVerts.push_back(iv);
    }
    
    auto internalSkeleton = skeleton->GetInternal();
    if (!internalSkeleton) return;
    
    animation::RiggingUtils::AssignDistanceWeights(
        internalVerts, internalSkeleton, boneIndicesPerVertex, falloffDistance, power);
    
    // Convert back to API vertices
    for (size_t i = 0; i < vertices.size() && i < internalVerts.size(); ++i) {
        const auto& iv = internalVerts[i];
        auto& v = vertices[i];
        v.boneIndices = iv.boneIndices;
        v.boneWeights = iv.boneWeights;
    }
}

void RiggingUtils::CalculateHeatDiffusionWeights(
    std::shared_ptr<Mesh> mesh,
    std::shared_ptr<Skeleton> skeleton,
    int iterations) {
    
    if (!mesh || !skeleton) return;
    
    auto internalMesh = mesh->GetInternal();
    auto internalSkeleton = skeleton->GetInternal();
    if (!internalMesh || !internalSkeleton) return;
    
    animation::RiggingUtils::CalculateHeatDiffusionWeights(
        internalMesh, internalSkeleton, iterations);
}

void RiggingUtils::AssignSingleBone(
    std::vector<Mesh::Vertex>& vertices,
    std::shared_ptr<Skeleton> skeleton,
    size_t boneIndex,
    float maxDistance) {
    
    if (!skeleton) return;
    
    // Convert API vertices to internal vertices
    std::vector<::eyelib::render::Vertex> internalVerts;
    internalVerts.reserve(vertices.size());
    for (const auto& v : vertices) {
        ::eyelib::render::Vertex iv;
        iv.position = v.position;
        iv.normal = v.normal;
        iv.uv = v.uv;
        iv.uv1 = v.uv1;
        iv.tangent = v.tangent;
        iv.color = v.color;
        iv.boneIndices = v.boneIndices;
        iv.boneWeights = v.boneWeights;
        internalVerts.push_back(iv);
    }
    
    auto internalSkeleton = skeleton->GetInternal();
    if (!internalSkeleton) return;
    
    animation::RiggingUtils::AssignSingleBone(
        internalVerts, internalSkeleton, boneIndex, maxDistance);
    
    // Convert back to API vertices
    for (size_t i = 0; i < vertices.size() && i < internalVerts.size(); ++i) {
        const auto& iv = internalVerts[i];
        auto& v = vertices[i];
        v.boneIndices = iv.boneIndices;
        v.boneWeights = iv.boneWeights;
    }
}

void RiggingUtils::NormalizeWeights(std::vector<Mesh::Vertex>& vertices) {
    // Convert API vertices to internal vertices
    std::vector<::eyelib::render::Vertex> internalVerts;
    internalVerts.reserve(vertices.size());
    for (const auto& v : vertices) {
        ::eyelib::render::Vertex iv;
        iv.position = v.position;
        iv.normal = v.normal;
        iv.uv = v.uv;
        iv.uv1 = v.uv1;
        iv.tangent = v.tangent;
        iv.color = v.color;
        iv.boneIndices = v.boneIndices;
        iv.boneWeights = v.boneWeights;
        internalVerts.push_back(iv);
    }
    
    animation::RiggingUtils::NormalizeWeights(internalVerts);
    
    // Convert back to API vertices
    for (size_t i = 0; i < vertices.size() && i < internalVerts.size(); ++i) {
        const auto& iv = internalVerts[i];
        auto& v = vertices[i];
        v.boneIndices = iv.boneIndices;
        v.boneWeights = iv.boneWeights;
    }
}

math::Vec3 RiggingUtils::GetBoneWorldPosition(
    std::shared_ptr<Skeleton> skeleton,
    size_t boneIndex) {
    
    if (!skeleton) return math::Vec3(0.0f);
    
    auto internalSkeleton = skeleton->GetInternal();
    if (!internalSkeleton) return math::Vec3(0.0f);
    
    return animation::RiggingUtils::GetBoneWorldPosition(internalSkeleton, boneIndex);
}

} // namespace api
} // namespace eyelib
