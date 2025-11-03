#include "AnimationLoader.h"
#include "core/log/Log.h"
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <algorithm>
#include <unordered_set>

namespace OpenEye {
namespace Animation {

std::shared_ptr<Skeleton> AnimationLoader::ExtractSkeleton(const aiScene* scene, size_t meshIndex) {
    if (!scene || !scene->mRootNode) {
        OE_ERROR("Cannot extract skeleton: invalid scene");
        return nullptr;
    }
    
    if (meshIndex >= scene->mNumMeshes) {
        OE_ERROR_F("Mesh index %zu out of range (%u meshes)", meshIndex, scene->mNumMeshes);
        return nullptr;
    }
    
    auto skeleton = std::make_shared<Skeleton>();
    std::unordered_map<std::string, int> nodeToBoneIndex;
    
    // First, extract bones from the mesh (these are the actual bones with weights)
    const aiMesh* mesh = scene->mMeshes[meshIndex];
    ExtractBonesFromMesh(mesh, scene, *skeleton, nodeToBoneIndex);
    
    // Then, process the node hierarchy to build complete skeleton structure
    ProcessBoneHierarchy(scene->mRootNode, scene, *skeleton, nodeToBoneIndex);
    
    // Build bone name mapping
    skeleton->BuildBoneMap();
    
    if (!skeleton->IsValid()) {
        OE_WARN("Extracted skeleton failed validation");
    }
    
    // Set global inverse transform from scene root (brings scene space to model space)
    // This helps fix FBX unit/axis/pivot differences and prevents stretching
    glm::mat4 sceneRoot = AssimpToGlm(scene->mRootNode->mTransformation);
    skeleton->SetGlobalInverseTransform(glm::inverse(sceneRoot));
    
    return skeleton;
}

void AnimationLoader::ExtractBoneWeights(const aiMesh* mesh,
                                         const Skeleton& skeleton,
                                         std::vector<glm::ivec4>& outBoneIndices,
                                         std::vector<glm::vec4>& outBoneWeights) {
    if (!mesh) {
        return;
    }
    
    size_t vertexCount = mesh->mNumVertices;
    outBoneIndices.resize(vertexCount, glm::ivec4(0));
    outBoneWeights.resize(vertexCount, glm::vec4(0.0f));
    
    // Process each bone in the mesh
    for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
        const aiBone* bone = mesh->mBones[i];
        std::string boneName = std::string(bone->mName.C_Str());
        
        int boneIndex = skeleton.FindBoneIndex(boneName);
        if (boneIndex < 0) {
            OE_WARN_F("Bone '%s' not found in skeleton", boneName.c_str());
            continue;
        }
        
        // Assign weights to vertices
        for (unsigned int j = 0; j < bone->mNumWeights; ++j) {
            const aiVertexWeight& weight = bone->mWeights[j];
            unsigned int vertexId = weight.mVertexId;
            
            if (vertexId >= vertexCount) {
                continue;
            }
            
            // Find first empty slot in bone weights (max 4 bones per vertex)
            glm::ivec4& indices = outBoneIndices[vertexId];
            glm::vec4& weights = outBoneWeights[vertexId];
            
            for (int slot = 0; slot < 4; ++slot) {
                if (weights[slot] == 0.0f) {
                    indices[slot] = boneIndex;
                    weights[slot] = weight.mWeight;
                    break;
                }
            }
        }
    }
    
    // Normalize weights (sum should be 1.0 per vertex)
    for (size_t i = 0; i < vertexCount; ++i) {
        glm::vec4& weights = outBoneWeights[i];
        float sum = weights.x + weights.y + weights.z + weights.w;
        if (sum > 0.0f) {
            weights /= sum;
        }
    }
}

std::vector<std::shared_ptr<AnimationClip>> AnimationLoader::ExtractAnimations(
    const aiScene* scene,
    const Skeleton& skeleton) {
    
    std::vector<std::shared_ptr<AnimationClip>> clips;
    
    if (!scene || !scene->mAnimations) {
        return clips;
    }
    
    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        const aiAnimation* anim = scene->mAnimations[i];
        
        std::string animName = anim->mName.length > 0 
            ? std::string(anim->mName.C_Str()) 
            : "Animation_" + std::to_string(i);
        
        // Calculate duration (in seconds)
        double ticksPerSecond = anim->mTicksPerSecond > 0.0 
            ? anim->mTicksPerSecond 
            : 25.0; // Default: 25 FPS
        float duration = static_cast<float>(anim->mDuration / ticksPerSecond);
        
        auto clip = std::make_shared<AnimationClip>(animName, duration);
        
        // Process each channel (each channel animates one bone)
        size_t matchedChannels = 0;
        size_t skippedChannels = 0;
        for (unsigned int j = 0; j < anim->mNumChannels; ++j) {
            const aiNodeAnim* channel = anim->mChannels[j];
            std::string boneName = std::string(channel->mNodeName.C_Str());
            
            int boneIndex = skeleton.FindBoneIndex(boneName);
            if (boneIndex < 0) {
                // Bone not in skeleton, skip this channel
                skippedChannels++;
                // Debug: Log once per unique missing bone (limit to first 10)
                static std::unordered_set<std::string> loggedMissingBones;
                if (loggedMissingBones.size() < 10 && loggedMissingBones.find(boneName) == loggedMissingBones.end()) {
                    OE_WARN_F("Animation channel references bone '%s' not found in skeleton (skipping channel)", boneName.c_str());
                    loggedMissingBones.insert(boneName);
                }
                continue;
            }
            
            matchedChannels++;
            AnimationChannel animChannel;
            animChannel.boneIndex = boneIndex;  // This is the skeleton bone index
            
            // Extract position keyframes
            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
                const aiVectorKey& key = channel->mPositionKeys[k];
                float time = static_cast<float>(key.mTime / ticksPerSecond);
                glm::vec3 pos = AssimpToGlm(key.mValue);
                animChannel.positionKeys.emplace_back(time, pos);
            }
            
            // Extract rotation keyframes
            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
                const aiQuatKey& key = channel->mRotationKeys[k];
                float time = static_cast<float>(key.mTime / ticksPerSecond);
                glm::quat rot = AssimpToGlm(key.mValue);
                animChannel.rotationKeys.emplace_back(time, rot);
            }
            
            // Extract scale keyframes
            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
                const aiVectorKey& key = channel->mScalingKeys[k];
                float time = static_cast<float>(key.mTime / ticksPerSecond);
                glm::vec3 scale = AssimpToGlm(key.mValue);
                animChannel.scaleKeys.emplace_back(time, scale);
            }
            
            clip->AddChannel(animChannel);
        }
        
        OE_INFO_F("Animation clip '%s': %zu channels total, %zu matched to skeleton bones, %zu skipped (unmatched bones)",
                 animName.c_str(), anim->mNumChannels, matchedChannels, skippedChannels);
        
        clips.push_back(clip);
    }
    
    return clips;
}

void AnimationLoader::ProcessBoneHierarchy(aiNode* node,
                                          const aiScene* scene,
                                          Skeleton& skeleton,
                                          std::unordered_map<std::string, int>& nodeToBoneIndex,
                                          int parentBoneIndex) {
    if (!node) {
        return;
    }
    
    std::string nodeName = std::string(node->mName.C_Str());
    int boneIndex = -1;
    
    // Check if this node is already a bone (from mesh extraction)
    auto it = nodeToBoneIndex.find(nodeName);
    if (it != nodeToBoneIndex.end()) {
        boneIndex = it->second;
        Bone& bone = skeleton.GetBone(boneIndex);
        
        // Update parent if needed
        if (bone.parentIndex == -1 && parentBoneIndex >= 0) {
            bone.parentIndex = parentBoneIndex;
        }
        
        // IMPORTANT: Update bind pose local transform from node hierarchy
        // The offsetMatrix from mesh extraction is NOT the local transform
        // Extract correct local transform from node->mTransformation
        glm::mat4 nodeTransform = AssimpToGlm(node->mTransformation);
        
        // Decompose into position, rotation, scale (manual decomposition)
        glm::mat4 m = nodeTransform;
        glm::vec3 scale(
            glm::length(glm::vec3(m[0])),
            glm::length(glm::vec3(m[1])),
            glm::length(glm::vec3(m[2]))
        );
        glm::mat3 rotMat(
            glm::vec3(m[0]) / scale.x,
            glm::vec3(m[1]) / scale.y,
            glm::vec3(m[2]) / scale.z
        );
        glm::quat rotation = glm::quat_cast(rotMat);
        glm::vec3 translation = glm::vec3(m[3]);
        
        // Update bind pose local transform (this is the correct one from hierarchy)
        bone.bindPosition = translation;
        bone.bindRotation = rotation;
        bone.bindScale = scale;
    } else {
        // Create new bone from node
        Bone bone;
        bone.name = nodeName;
        bone.parentIndex = parentBoneIndex;
        
        // Extract transform from node
        glm::mat4 nodeTransform = AssimpToGlm(node->mTransformation);
        
        // Decompose into position, rotation, scale (manual decomposition)
        glm::mat4 m = nodeTransform;
        glm::vec3 scale(
            glm::length(glm::vec3(m[0])),
            glm::length(glm::vec3(m[1])),
            glm::length(glm::vec3(m[2]))
        );
        glm::mat3 rotMat(
            glm::vec3(m[0]) / scale.x,
            glm::vec3(m[1]) / scale.y,
            glm::vec3(m[2]) / scale.z
        );
        glm::quat rotation = glm::quat_cast(rotMat);
        glm::vec3 translation = glm::vec3(m[3]);
        
        bone.bindPosition = translation;
        bone.bindRotation = rotation;
        bone.bindScale = scale;
        
        boneIndex = static_cast<int>(skeleton.GetBoneCount());
        skeleton.AddBone(bone);
        nodeToBoneIndex[nodeName] = boneIndex;
    }
    
    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessBoneHierarchy(node->mChildren[i], scene, skeleton, nodeToBoneIndex, boneIndex);
    }
}

void AnimationLoader::ExtractBonesFromMesh(const aiMesh* mesh,
                                          const aiScene* scene,
                                          Skeleton& skeleton,
                                          std::unordered_map<std::string, int>& nodeToBoneIndex) {
    if (!mesh || !mesh->mBones) {
        return;
    }
    
    for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
        const aiBone* bone = mesh->mBones[i];
        std::string boneName = std::string(bone->mName.C_Str());
        
        // Check if bone already exists
        if (nodeToBoneIndex.find(boneName) != nodeToBoneIndex.end()) {
            continue;
        }
        
        // Create bone
        Bone newBone;
        newBone.name = boneName;
        newBone.parentIndex = -1; // Will be set by hierarchy processing
        
        // Extract inverse bind matrix from bone offset matrix
        // IMPORTANT: Assimp's mOffsetMatrix transforms from bone space to mesh space (bind pose)
        // This is NOT the bone's local transform - it's a world-space transform
        // We only use it to calculate the inverse bind matrix for skinning
        glm::mat4 offsetMatrix = AssimpToGlm(bone->mOffsetMatrix);
        
        // aiBone::mOffsetMatrix already transforms from mesh bind pose to bone space (inverse bind)
        // Use it directly for skinning
        newBone.inverseBindMatrix = offsetMatrix;
        
        // DO NOT extract bind pose local transform from offsetMatrix!
        // The bind pose local transform will be extracted from the node hierarchy
        // in ProcessBoneHierarchy, which uses node->mTransformation (correct local transform)
        // Initialize to identity for now - will be set by ProcessBoneHierarchy
        newBone.bindPosition = glm::vec3(0.0f);
        newBone.bindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        newBone.bindScale = glm::vec3(1.0f);
        
        int boneIndex = static_cast<int>(skeleton.GetBoneCount());
        skeleton.AddBone(newBone);
        nodeToBoneIndex[boneName] = boneIndex;
    }
}

glm::mat4 AnimationLoader::AssimpToGlm(const aiMatrix4x4& matrix) {
    glm::mat4 result;
    // Assimp uses row-major, GLM uses column-major
    result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
    result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
    result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
    result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
    return result;
}

glm::vec3 AnimationLoader::AssimpToGlm(const aiVector3D& vec) {
    return glm::vec3(vec.x, vec.y, vec.z);
}

glm::quat AnimationLoader::AssimpToGlm(const aiQuaternion& quat) {
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

} // namespace Animation
} // namespace OpenEye

