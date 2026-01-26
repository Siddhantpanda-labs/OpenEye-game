#include "eyelib/assets/loaders/ModelLoader.h"

#include "eyelib/animation/KeyframeEditor.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace eyelib {
namespace assets {

namespace {

bool IsGLTF(const std::string& path) {
    if (path.size() < 4) return false;
    std::string s = path;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if (s.size() >= 5 && s.substr(s.size() - 5) == ".gltf") return true;
    if (s.size() >= 4 && s.substr(s.size() - 4) == ".glb") return true;
    return false;
}

math::Mat4 ToMat4(const aiMatrix4x4& m) {
    math::Mat4 out{1.0f};
    out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
    out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
    out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
    out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
    return out;
}

void DecomposeTRS(const math::Mat4& m, math::Vec3& outT, math::Quat& outR, math::Vec3& outS) {
    // Use the engine's standard decomposition to preserve handedness/sign.
    math::Decompose(m, outT, outR, outS);
    outR = glm::normalize(outR);
}

void ExtractBonesFromMeshes(const aiScene* scene,
                            animation::Skeleton& skeleton,
                            std::unordered_map<std::string, int>& nodeToBoneIndex) {
    if (!scene || !scene->HasMeshes()) return;
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        if (!mesh || !mesh->HasBones()) continue;
        for (unsigned b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            const std::string boneName = bone->mName.C_Str();
            auto it = nodeToBoneIndex.find(boneName);
            if (it != nodeToBoneIndex.end()) {
                skeleton.GetBone(static_cast<size_t>(it->second)).inverseBindMatrix = ToMat4(bone->mOffsetMatrix);
                continue;
            }
            animation::Bone newBone;
            newBone.name = boneName;
            newBone.parentIndex = -1;
            newBone.inverseBindMatrix = ToMat4(bone->mOffsetMatrix);
            newBone.bindPosition = math::Vec3(0.0f);
            newBone.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
            newBone.bindScale = math::Vec3(1.0f);
            const int newIndex = static_cast<int>(skeleton.BoneCount());
            skeleton.AddBone(newBone);
            nodeToBoneIndex[boneName] = newIndex;
        }
    }
}

void ProcessBoneHierarchy(aiNode* node,
                          animation::Skeleton& skeleton,
                          std::unordered_map<std::string, int>& nodeToBoneIndex,
                          int parentBoneIndex) {
    if (!node) return;

    const std::string nodeName = node->mName.C_Str();
    int boneIndex = -1;
    auto it = nodeToBoneIndex.find(nodeName);
    if (it != nodeToBoneIndex.end()) {
        boneIndex = it->second;
        auto& bone = skeleton.GetBone(static_cast<size_t>(boneIndex));
        if (bone.parentIndex == -1 && parentBoneIndex >= 0) {
            bone.parentIndex = parentBoneIndex;
        }
    } else {
        animation::Bone newBone;
        newBone.name = nodeName;
        newBone.parentIndex = parentBoneIndex;
        newBone.inverseBindMatrix = math::Mat4(1.0f);
        newBone.bindPosition = math::Vec3(0.0f);
        newBone.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
        newBone.bindScale = math::Vec3(1.0f);
        boneIndex = static_cast<int>(skeleton.BoneCount());
        skeleton.AddBone(newBone);
        nodeToBoneIndex[nodeName] = boneIndex;
    }

    if (boneIndex >= 0) {
        math::Mat4 nodeTransform = ToMat4(node->mTransformation);
        math::Vec3 scale(
            glm::length(math::Vec3(nodeTransform[0])),
            glm::length(math::Vec3(nodeTransform[1])),
            glm::length(math::Vec3(nodeTransform[2]))
        );
        math::Mat3 rotMat(
            math::Vec3(nodeTransform[0]) / (scale.x != 0.0f ? scale.x : 1.0f),
            math::Vec3(nodeTransform[1]) / (scale.y != 0.0f ? scale.y : 1.0f),
            math::Vec3(nodeTransform[2]) / (scale.z != 0.0f ? scale.z : 1.0f)
        );
        math::Quat rotation = glm::quat_cast(rotMat);
        math::Vec3 translation = math::Vec3(nodeTransform[3]);

        auto& bone = skeleton.GetBone(static_cast<size_t>(boneIndex));
        bone.bindPosition = translation;
        bone.bindRotation = rotation;
        bone.bindScale = scale;
    }

    const int nextParent = (boneIndex >= 0) ? boneIndex : parentBoneIndex;
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        ProcessBoneHierarchy(node->mChildren[i], skeleton, nodeToBoneIndex, nextParent);
    }
}

// Unified skeleton construction: treats static models as degenerate animated case (zero animation tracks)
// Returns skeleton and mesh-to-bone mapping for static meshes
struct UnifiedSkeletonResult {
    std::shared_ptr<animation::Skeleton> skeleton;
    std::vector<int> meshToBoneIndex; // For static meshes: which bone index each mesh uses
};

UnifiedSkeletonResult BuildUnifiedSkeleton(const aiScene* scene, 
                                          const std::vector<math::Mat4>& meshGlobals) {
    UnifiedSkeletonResult result;
    
    if (!scene || !scene->mRootNode) {
        auto skeleton = std::make_shared<animation::Skeleton>();
        animation::Bone root;
        root.name = "Root";
        root.parentIndex = -1;
        root.inverseBindMatrix = math::Mat4(1.0f);
        root.bindPosition = math::Vec3(0.0f);
        root.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
        root.bindScale = math::Vec3(1.0f);
        skeleton->AddBone(root);
        skeleton->SetGlobalInverseTransform(math::Mat4(1.0f));
        skeleton->BuildBoneMap();
        result.skeleton = skeleton;
        result.meshToBoneIndex.assign(meshGlobals.size(), 0);
        return result;
    }

    // Check if any meshes have bones (animated path)
    bool hasBones = false;
    if (scene->HasMeshes()) {
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            if (scene->mMeshes[i] && scene->mMeshes[i]->HasBones()) {
                hasBones = true;
                break;
            }
        }
    }

    auto skeleton = std::make_shared<animation::Skeleton>();
    const math::Mat4 rootTransform = ToMat4(scene->mRootNode->mTransformation);

    result.meshToBoneIndex.assign(meshGlobals.size(), 0);

    if (hasBones) {
        // Animated path: extract bones from meshes
        // Use Assimp's root transform for global inverse (standard skinning path).
        skeleton->SetGlobalInverseTransform(glm::inverse(rootTransform));
        std::unordered_map<std::string, int> nodeToBoneIndex;
        ExtractBonesFromMeshes(scene, *skeleton, nodeToBoneIndex);
        ProcessBoneHierarchy(scene->mRootNode, *skeleton, nodeToBoneIndex, -1);
    } else {
        // Static path: create placement bones from scene graph
        // Use identity global inverse so we can feed raw mesh globals directly into the palette.
        skeleton->SetGlobalInverseTransform(math::Mat4(1.0f));
        // Root bone
        animation::Bone root;
        root.name = "Root";
        root.parentIndex = -1;
        root.inverseBindMatrix = math::Mat4(1.0f);
        root.bindPosition = math::Vec3(0.0f);
        root.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
        root.bindScale = math::Vec3(1.0f);
        skeleton->AddBone(root);

        // Create one placement bone per mesh
        for (unsigned i = 0; i < meshGlobals.size() && i < scene->mNumMeshes; ++i) {
            animation::Bone b;
            b.name = "Mesh_" + std::to_string(i);
            b.parentIndex = 0; // Parent to root
            // Feed raw node matrix directly into the palette (no TRS decomposition).
            b.inverseBindMatrix = meshGlobals[i];
            b.bindPosition = math::Vec3(0.0f);
            b.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
            b.bindScale = math::Vec3(1.0f);
            const int boneIndex = static_cast<int>(skeleton->BoneCount());
            skeleton->AddBone(b);
            result.meshToBoneIndex[i] = boneIndex;
        }
    }

    skeleton->BuildBoneMap();
    result.skeleton = skeleton;
    return result;
}

std::vector<std::shared_ptr<animation::AnimationClip>> BuildClips(
    const aiScene* scene, const animation::Skeleton& skeleton) {
    std::vector<std::shared_ptr<animation::AnimationClip>> clips;
    if (!scene) return clips;

    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* anim = scene->mAnimations[a];
        const double ticks = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
        const float duration = static_cast<float>(anim->mDuration / ticks);
        auto clip = std::make_shared<animation::AnimationClip>(anim->mName.C_Str(), duration);

        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* channel = anim->mChannels[c];
            const int boneIndex = skeleton.FindBoneIndex(channel->mNodeName.C_Str());
            if (boneIndex < 0) continue;

            for (unsigned i = 0; i < channel->mNumPositionKeys; ++i) {
                const auto& key = channel->mPositionKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    animation::KeyframeEditor::Track::Position);
            }
            for (unsigned i = 0; i < channel->mNumRotationKeys; ++i) {
                const auto& key = channel->mRotationKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z));
            }
            for (unsigned i = 0; i < channel->mNumScalingKeys; ++i) {
                const auto& key = channel->mScalingKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    animation::KeyframeEditor::Track::Scale);
            }
        }
        clips.push_back(clip);
    }
    return clips;
}

void AddBoneWeight(math::IVec4& indices, math::Vec4& weights, int boneIndex, float weight) {
    for (int i = 0; i < 4; ++i) {
        if (weights[i] == 0.0f) {
            indices[i] = boneIndex;
            weights[i] = weight;
            return;
        }
    }
    int minIndex = 0;
    float minWeight = weights[0];
    for (int i = 1; i < 4; ++i) {
        if (weights[i] < minWeight) {
            minWeight = weights[i];
            minIndex = i;
        }
    }
    if (weight > minWeight) {
        indices[minIndex] = boneIndex;
        weights[minIndex] = weight;
    }
}

void ExtractBoneWeights(const aiMesh* mesh,
                        const animation::Skeleton& skeleton,
                        std::vector<math::IVec4>& outBoneIndices,
                        std::vector<math::Vec4>& outBoneWeights) {
    const size_t vertexCount = mesh ? mesh->mNumVertices : 0;
    outBoneIndices.assign(vertexCount, math::IVec4(0));
    outBoneWeights.assign(vertexCount, math::Vec4(0.0f));
    if (!mesh || !mesh->HasBones()) return;

    for (unsigned i = 0; i < mesh->mNumBones; ++i) {
        const aiBone* bone = mesh->mBones[i];
        const int boneIndex = skeleton.FindBoneIndex(bone->mName.C_Str());
        if (boneIndex < 0) continue;
        for (unsigned j = 0; j < bone->mNumWeights; ++j) {
            const aiVertexWeight& vw = bone->mWeights[j];
            if (vw.mVertexId >= vertexCount) continue;
            AddBoneWeight(outBoneIndices[vw.mVertexId], outBoneWeights[vw.mVertexId], boneIndex, vw.mWeight);
        }
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        float sum = outBoneWeights[i].x + outBoneWeights[i].y + outBoneWeights[i].z + outBoneWeights[i].w;
        if (sum > 0.0f) {
            outBoneWeights[i] /= sum;
        }
    }
}

std::vector<math::Mat4> BuildBindPoseMatrices(const animation::Skeleton& skeleton) {
    const size_t count = skeleton.BoneCount();
    std::vector<math::Mat4> local(count, math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        const auto& bone = skeleton.GetBone(i);
        local[i] = math::Compose(bone.bindPosition, bone.bindRotation, bone.bindScale);
    }

    std::vector<math::Mat4> global(count, math::Mat4(1.0f));
    std::vector<uint8_t> computed(count, 0);
    std::function<void(size_t)> computeGlobal = [&](size_t idx) {
        if (computed[idx]) return;
        const auto& bone = skeleton.GetBone(idx);
        if (bone.parentIndex >= 0) {
            const size_t parent = static_cast<size_t>(bone.parentIndex);
            if (parent < count) {
                computeGlobal(parent);
                global[idx] = global[parent] * local[idx];
            } else {
                global[idx] = local[idx];
            }
        } else {
            global[idx] = local[idx];
        }
        computed[idx] = 1;
    };

    std::vector<math::Mat4> out(count, math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        computeGlobal(i);
        const auto& bone = skeleton.GetBone(i);
        out[i] = skeleton.GetGlobalInverseTransform() * global[i] * bone.inverseBindMatrix;
    }

    return out;
}

void CollectMeshTransforms(const aiNode* node,
                           const math::Mat4& parentTransform,
                           std::vector<math::Mat4>& outMeshTransforms) {
    if (!node) return;
    math::Mat4 local = ToMat4(node->mTransformation);
    math::Mat4 global = parentTransform * local;
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        const unsigned meshIndex = node->mMeshes[i];
        if (meshIndex < outMeshTransforms.size()) {
            outMeshTransforms[meshIndex] = global;
        }
    }
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        CollectMeshTransforms(node->mChildren[i], global, outMeshTransforms);
    }
}

std::shared_ptr<render::Mesh> BuildMesh(const aiMesh* ai,
                                        animation::Skeleton* skeleton,
                                        int materialIndex,
                                        int defaultBoneIndex) {
    auto mesh = std::make_shared<render::Mesh>();
    std::vector<math::IVec4> boneIndicesPerVertex;
    std::vector<math::Vec4> boneWeightsPerVertex;
    if (ai && ai->HasBones() && skeleton) {
        ExtractBoneWeights(ai, *skeleton, boneIndicesPerVertex, boneWeightsPerVertex);
    }

    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.resize(ai->mNumVertices);

    for (unsigned i = 0; i < ai->mNumVertices; ++i) {
        auto& v = vertices[i];
        math::Vec3 pos(ai->mVertices[i].x, ai->mVertices[i].y, ai->mVertices[i].z);
        v.position = pos;
        if (ai->mNormals) {
            math::Vec3 n(ai->mNormals[i].x, ai->mNormals[i].y, ai->mNormals[i].z);
            v.normal = glm::normalize(n);
        }
        if (ai->mTextureCoords[0]) {
            v.uv = math::Vec2(ai->mTextureCoords[0][i].x, ai->mTextureCoords[0][i].y);
        }
        if (ai->mTextureCoords[1]) {
            v.uv1 = math::Vec2(ai->mTextureCoords[1][i].x, ai->mTextureCoords[1][i].y);
        }
        if (ai->mTangents) {
            math::Vec3 t(ai->mTangents[i].x, ai->mTangents[i].y, ai->mTangents[i].z);
            v.tangent = glm::normalize(t);
        }
        v.color = math::Vec4(1, 1, 1, 1);
        if (!boneIndicesPerVertex.empty() && i < boneIndicesPerVertex.size()) {
            v.boneIndices = boneIndicesPerVertex[i];
            v.boneWeights = boneWeightsPerVertex[i];
        } else {
            v.boneIndices = math::IVec4(defaultBoneIndex, 0, 0, 0);
            v.boneWeights = math::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    indices.reserve(ai->mNumFaces * 3);
    for (unsigned i = 0; i < ai->mNumFaces; ++i) {
        const aiFace& f = ai->mFaces[i];
        for (unsigned j = 0; j < f.mNumIndices; ++j) {
            indices.push_back(f.mIndices[j]);
        }
    }

    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->ClearSubmeshes();
    mesh->AddSubmesh(0, static_cast<uint32_t>(indices.size()), materialIndex);
    mesh->UploadToGPU();
    return mesh;
}

// Generic texture loader for any texture type
std::shared_ptr<render::Texture2D> LoadTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir,
    aiTextureType type, bool sRGB = true) {
    if (!scene || materialIndex >= scene->mNumMaterials) return nullptr;
    const aiMaterial* mat = scene->mMaterials[materialIndex];
    aiString path;
    if (mat->GetTextureCount(type) == 0 ||
        mat->GetTexture(type, 0, &path) != aiReturn_SUCCESS) {
        return nullptr;
    }

    std::string texPath = path.C_Str();
    if (texPath.empty()) return nullptr;

    // 1) Embedded textures (FBX can embed or use "*0" style references)
    const aiTexture* embedded = nullptr;
    if (!texPath.empty() && texPath[0] == '*') {
        const std::string indexStr = texPath.substr(1);
        char* endPtr = nullptr;
        const long index = std::strtol(indexStr.c_str(), &endPtr, 10);
        if (endPtr != indexStr.c_str() && index >= 0 && static_cast<unsigned>(index) < scene->mNumTextures) {
            embedded = scene->mTextures[static_cast<unsigned>(index)];
        }
    } else if (scene->mNumTextures > 0) {
        embedded = scene->GetEmbeddedTexture(texPath.c_str());
    }

    if (embedded) {
        auto tex = std::make_shared<render::Texture2D>();
        if (embedded->mHeight == 0) {
            // Compressed image data (PNG/JPG); mWidth is data size in bytes.
            if (tex->LoadFromMemory(reinterpret_cast<const uint8_t*>(embedded->pcData),
                                    static_cast<size_t>(embedded->mWidth),
                                    sRGB, true)) {
                return tex;
            }
        } else {
            // Raw BGRA8888 pixels.
            const int width = static_cast<int>(embedded->mWidth);
            const int height = static_cast<int>(embedded->mHeight);
            const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<uint8_t> rgba;
            rgba.resize(count * 4);
            const aiTexel* src = embedded->pcData;
            for (size_t i = 0; i < count; ++i) {
                rgba[i * 4 + 0] = src[i].r;
                rgba[i * 4 + 1] = src[i].g;
                rgba[i * 4 + 2] = src[i].b;
                rgba[i * 4 + 3] = src[i].a;
            }
            if (tex->Create(width, height, render::TextureFormat::RGBA8, rgba.data())) {
                tex->GenerateMipmaps();
                return tex;
            }
        }
    }

    // 2) External file resolution with fallback search paths
    std::vector<std::string> candidates;
    if (platform::filesystem::FileExists(texPath)) {
        candidates.push_back(texPath);
    }

    if (!baseDir.empty()) {
        candidates.push_back(platform::filesystem::NormalizePath(baseDir + "/" + texPath));
        const std::string filename = platform::filesystem::GetFilename(texPath);
        candidates.push_back(platform::filesystem::NormalizePath(baseDir + "/" + filename));
        const char* subdirs[] = {"fbm", "textures", "maps", "tex", "images", "img"};
        for (const char* subdir : subdirs) {
            candidates.push_back(platform::filesystem::NormalizePath(baseDir + "/" + subdir + "/" + filename));
        }
    }

    for (const auto& candidate : candidates) {
        if (!platform::filesystem::FileExists(candidate)) continue;
        auto tex = std::make_shared<render::Texture2D>();
        if (tex->LoadFromFile(candidate, sRGB, true)) {
            return tex;
        }
    }

    return nullptr;
}

// Convenience wrappers for specific texture types
std::shared_ptr<render::Texture2D> LoadAlbedoTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    // Try BASE_COLOR first (glTF), then DIFFUSE (FBX)
    auto tex = LoadTexture(scene, materialIndex, baseDir, aiTextureType_BASE_COLOR, true);
    if (tex) return tex;
    return LoadTexture(scene, materialIndex, baseDir, aiTextureType_DIFFUSE, true);
}

std::shared_ptr<render::Texture2D> LoadNormalTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    // Try NORMALS first, then HEIGHT (sometimes used as normal)
    auto tex = LoadTexture(scene, materialIndex, baseDir, aiTextureType_NORMALS, false);
    if (tex) return tex;
    return LoadTexture(scene, materialIndex, baseDir, aiTextureType_HEIGHT, false);
}

std::shared_ptr<render::Texture2D> LoadMetallicRoughnessTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    // glTF uses UNKNOWN for metallic-roughness (combined texture)
    // Try METALNESS and ROUGHNESS separately first
    auto metallicTex = LoadTexture(scene, materialIndex, baseDir, aiTextureType_METALNESS, false);
    auto roughnessTex = LoadTexture(scene, materialIndex, baseDir, aiTextureType_DIFFUSE_ROUGHNESS, false);
    
    // For glTF, metallic-roughness is often in UNKNOWN slot
    // We'll check by texture name pattern
    if (!scene || materialIndex >= scene->mNumMaterials) return nullptr;
    const aiMaterial* mat = scene->mMaterials[materialIndex];
    
    // Check all texture types for metallic-roughness by name
    for (int texType = 1; texType < AI_TEXTURE_TYPE_MAX; ++texType) {
        if (mat->GetTextureCount(static_cast<aiTextureType>(texType)) > 0) {
            aiString texPath;
            if (mat->GetTexture(static_cast<aiTextureType>(texType), 0, &texPath) == aiReturn_SUCCESS) {
                std::string pathStr = texPath.C_Str();
                std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
                if (pathStr.find("metallicroughness") != std::string::npos ||
                    (pathStr.find("metallic") != std::string::npos && pathStr.find("roughness") != std::string::npos)) {
                    return LoadTexture(scene, materialIndex, baseDir, static_cast<aiTextureType>(texType), false);
                }
            }
        }
    }
    
    return nullptr;
}

std::shared_ptr<render::Texture2D> LoadAOTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    auto tex = LoadTexture(scene, materialIndex, baseDir, aiTextureType_AMBIENT_OCCLUSION, false);
    if (tex) return tex;
    return LoadTexture(scene, materialIndex, baseDir, aiTextureType_LIGHTMAP, false);
}

std::shared_ptr<render::Texture2D> LoadEmissiveTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    return LoadTexture(scene, materialIndex, baseDir, aiTextureType_EMISSIVE, true);
}

struct MaterialUVInfo {
    math::Vec4 transform{0.0f, 0.0f, 1.0f, 1.0f}; // offset.xy, scale.zw
    float rotation = 0.0f; // radians
    int uvIndex = 0;
};

MaterialUVInfo GetMaterialUVInfo(const aiMaterial* mat, aiTextureType type) {
    MaterialUVInfo info;
    if (!mat) return info;

    int uvIndex = 0;
    if (mat->Get(AI_MATKEY_UVWSRC(type, 0), uvIndex) == aiReturn_SUCCESS) {
        info.uvIndex = uvIndex;
    }

    aiUVTransform uvTransform;
    if (mat->Get(AI_MATKEY_UVTRANSFORM(type, 0), uvTransform) == aiReturn_SUCCESS) {
        info.transform = math::Vec4(
            uvTransform.mTranslation.x,
            uvTransform.mTranslation.y,
            uvTransform.mScaling.x,
            uvTransform.mScaling.y
        );
        info.rotation = uvTransform.mRotation;
    }

    return info;
}

} // namespace

LoadedModel ModelLoader::LoadModel(const std::string& path) {
    LoadedModel result;

    Assimp::Importer importer;
    unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
    if (!IsGLTF(path)) {
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_JoinIdenticalVertices;
    }

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) {
        core::Log::WarnF("ModelLoader: failed to load %s", path.c_str());
        return result;
    }

    // Collect mesh global transforms from scene graph.
    result.meshes.reserve(scene->mNumMeshes);
    std::vector<math::Mat4> meshGlobals;
    meshGlobals.resize(scene->mNumMeshes, math::Mat4(1.0f));
    CollectMeshTransforms(scene->mRootNode, math::Mat4(1.0f), meshGlobals);

    // Unity-style invariant (strict): no vertex baking.
    // Unified skeleton construction: treats static models as degenerate animated case (zero animation tracks).
    // - Skinned meshes: use aiBone weights + inverse bind matrices.
    // - Static meshes: create per-mesh "placement bones" and weight vertices to those bones (w=1).
    //
    // meshTransforms exported to API remain identity for all meshes; spatial meaning comes from bones.
    result.meshTransforms.assign(scene->mNumMeshes, math::Mat4(1.0f));

    // Unified skeleton construction (single path for both animated and static models)
    UnifiedSkeletonResult skeletonResult = BuildUnifiedSkeleton(scene, meshGlobals);
    result.skeleton = skeletonResult.skeleton;
    
    // Build animation clips (will be empty for static models)
    auto clips = BuildClips(scene, *result.skeleton);
    result.clip = clips.empty() ? nullptr : clips.front();

    const std::string baseDir = platform::filesystem::GetDirectory(path);
    std::shared_ptr<render::Shader> skinnedShader = std::make_shared<render::Shader>();
    skinnedShader->LoadFromFile("assets/shaders/passes/forward/skinned.vert",
                                "assets/shaders/passes/forward/skinned.frag");

    // Default textures for materials without textures
    auto defaultAlbedo = std::make_shared<render::Texture2D>();
    {
        const uint8_t white[4] = {255, 255, 255, 255};
        defaultAlbedo->Create(1, 1, render::TextureFormat::RGBA8, white);
    }
    auto defaultNormal = std::make_shared<render::Texture2D>();
    {
        const uint8_t normal[4] = {128, 128, 255, 255}; // Normal pointing up (0.5, 0.5, 1.0) -> (128, 128, 255)
        defaultNormal->Create(1, 1, render::TextureFormat::RGBA8, normal);
    }
    auto defaultMetallicRoughness = std::make_shared<render::Texture2D>();
    {
        const uint8_t mr[4] = {0, 128, 0, 255}; // Metallic=0, Roughness=0.5
        defaultMetallicRoughness->Create(1, 1, render::TextureFormat::RGBA8, mr);
    }
    auto defaultAO = std::make_shared<render::Texture2D>();
    {
        const uint8_t ao[4] = {255, 255, 255, 255}; // AO=1.0
        defaultAO->Create(1, 1, render::TextureFormat::RGBA8, ao);
    }
    auto defaultEmissive = std::make_shared<render::Texture2D>();
    {
        const uint8_t emissive[4] = {0, 0, 0, 255}; // No emission
        defaultEmissive->Create(1, 1, render::TextureFormat::RGBA8, emissive);
    }

    const size_t materialCount = scene->mNumMaterials;
    std::vector<std::shared_ptr<render::Texture2D>> albedos;
    albedos.resize(materialCount);
    for (unsigned i = 0; i < materialCount; ++i) {
        auto albedo = LoadAlbedoTexture(scene, i, baseDir);
        albedos[i] = albedo ? albedo : defaultAlbedo;
    }

    // According to advise.md: Materials must be transform-agnostic
    // Build bind pose matrices (shared for all materials)
    const std::vector<math::Mat4> bindPoseMatrices = BuildBindPoseMatrices(*result.skeleton);
    
    const bool isGltf = IsGLTF(path);
    
    // Load PBR textures
    std::vector<std::shared_ptr<render::Texture2D>> normals, metallicRoughness, ao, emissive;
    normals.resize(materialCount);
    metallicRoughness.resize(materialCount);
    ao.resize(materialCount);
    emissive.resize(materialCount);
    
    for (unsigned i = 0; i < materialCount; ++i) {
        normals[i] = LoadNormalTexture(scene, i, baseDir);
        metallicRoughness[i] = LoadMetallicRoughnessTexture(scene, i, baseDir);
        ao[i] = LoadAOTexture(scene, i, baseDir);
        emissive[i] = LoadEmissiveTexture(scene, i, baseDir);
    }
    
    result.materials.resize(materialCount);
    for (unsigned i = 0; i < materialCount; ++i) {
        const aiMaterial* aiMat = scene->mMaterials[i];
        auto mat = std::make_shared<render::Material>();
        mat->SetShader(skinnedShader);
        
        // Albedo texture (always set, has default)
        mat->SetAlbedoTexture(albedos[i]);
        
        // PBR textures (set defaults if missing, set flags)
        if (normals[i]) {
            mat->SetNormalTexture(normals[i]);
            mat->SetFloat("u_HasNormalTexture", 1.0f);
        } else {
            mat->SetNormalTexture(defaultNormal);
            mat->SetFloat("u_HasNormalTexture", 0.0f);
        }
        if (metallicRoughness[i]) {
            mat->SetMetallicRoughnessTexture(metallicRoughness[i]);
            mat->SetFloat("u_HasMetallicRoughnessTexture", 1.0f);
        } else {
            mat->SetMetallicRoughnessTexture(defaultMetallicRoughness);
            mat->SetFloat("u_HasMetallicRoughnessTexture", 0.0f);
        }
        if (ao[i]) {
            mat->SetAOTexture(ao[i]);
            mat->SetFloat("u_HasAOTexture", 1.0f);
        } else {
            mat->SetAOTexture(defaultAO);
            mat->SetFloat("u_HasAOTexture", 0.0f);
        }
        if (emissive[i]) {
            mat->SetEmissiveTexture(emissive[i]);
            mat->SetFloat("u_HasEmissiveTexture", 1.0f);
        } else {
            mat->SetEmissiveTexture(defaultEmissive);
            mat->SetFloat("u_HasEmissiveTexture", 0.0f);
        }
        
        // Load PBR material properties
        aiColor3D color;
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
            mat->SetAlbedo(math::Vec3(color.r, color.g, color.b));
        }
        
        float metallic = 0.0f;
        if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
            mat->SetMetallic(metallic);
        }
        
        float roughness = 0.5f;
        if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
            mat->SetRoughness(roughness);
        }
        
        if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == aiReturn_SUCCESS) {
            mat->SetEmission(math::Vec3(color.r, color.g, color.b));
        }
        
        float ao = 1.0f;
        // AO is typically in a texture, but some formats may have it as a material property
        // For now, default to 1.0 (no occlusion)
        mat->SetAO(ao);
        
        // glTF UV transform support (KHR_texture_transform) for base color.
        MaterialUVInfo uvInfo = GetMaterialUVInfo(aiMat, aiTextureType_BASE_COLOR);
        if (aiMat->GetTextureCount(aiTextureType_BASE_COLOR) == 0) {
            // Fall back to diffuse slot if base color isn't used.
            uvInfo = GetMaterialUVInfo(aiMat, aiTextureType_DIFFUSE);
        }
        if (uvInfo.uvIndex > 1) {
            core::Log::WarnF("ModelLoader: material %u uses texCoord %d (only UV0/UV1 supported)",
                             i, uvInfo.uvIndex);
        }
        mat->SetVec4("u_UVTransform", uvInfo.transform);
        mat->SetFloat("u_UVRotation", uvInfo.rotation);
        mat->SetFloat("u_TexCoordIndex", uvInfo.uvIndex == 1 ? 1.0f : 0.0f);
        mat->SetFloat("u_FlipV", isGltf ? 1.0f : 0.0f);
        
        // Set PBR uniforms
        mat->SetFloat("u_Metallic", mat->GetMetallic());
        mat->SetFloat("u_Roughness", mat->GetRoughness());
        mat->SetVec3("u_Emission", mat->GetEmission());
        mat->SetFloat("u_AO", mat->GetAO());
        
        // According to advise.md: bindPoseMatrices always exists (at least root bone)
        mat->SetMat4Array("u_Bones", bindPoseMatrices);
        result.materials[i] = mat;
    }

    // Build meshes:
    // - Skinned meshes: per-vertex weights from aiBones.
    // - Static meshes: all vertices weighted to that mesh's placement bone (w=1).
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* aiMesh = scene->mMeshes[i];
        if (!aiMesh) continue;

        const int matIndex = static_cast<int>(aiMesh->mMaterialIndex);
        // For skinned meshes, bone weights come from aiBones (defaultBoneIndex not used).
        // For static meshes, use the placement bone index from unified skeleton.
        const int defaultBoneIndex = (aiMesh->HasBones()) ? 0
                                                          : ((i < skeletonResult.meshToBoneIndex.size()) 
                                                             ? skeletonResult.meshToBoneIndex[i] : 0);
        result.meshes.push_back(BuildMesh(aiMesh, result.skeleton.get(), matIndex, defaultBoneIndex));
    }

    return result;
}

} // namespace assets
} // namespace eyelib
