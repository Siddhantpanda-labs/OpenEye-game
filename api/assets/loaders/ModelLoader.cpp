#include "api/assets/loaders/ModelLoader.h"

#include "api/animation/AnimationClip.h"
#include "api/animation/Skeleton.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "api/render/Shader.h"
#include "api/render/Texture.h"

#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/KeyframeEditor.h"
#include "eyelib/animation/Skeleton.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace eyelib {
namespace api {

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

void ExtractBonesFromMeshes(const aiScene* scene,
                            ::eyelib::animation::Skeleton& skeleton,
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
            ::eyelib::animation::Bone newBone;
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
                          ::eyelib::animation::Skeleton& skeleton,
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

std::shared_ptr<::eyelib::animation::Skeleton> BuildSkeleton(const aiScene* scene) {
    if (!scene || !scene->mRootNode) return nullptr;
    bool hasBones = false;
    if (scene->HasMeshes()) {
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            if (scene->mMeshes[i] && scene->mMeshes[i]->HasBones()) {
                hasBones = true;
                break;
            }
        }
    }
    if (!hasBones) return nullptr;

    auto skeleton = std::make_shared<::eyelib::animation::Skeleton>();
    const math::Mat4 rootTransform = ToMat4(scene->mRootNode->mTransformation);
    skeleton->SetGlobalInverseTransform(glm::inverse(rootTransform));

    std::unordered_map<std::string, int> nodeToBoneIndex;
    ExtractBonesFromMeshes(scene, *skeleton, nodeToBoneIndex);

    // Ensure animation channel nodes exist in the hierarchy so parent transforms apply.
    if (scene->mNumAnimations > 0) {
        for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
            const aiAnimation* anim = scene->mAnimations[a];
            if (!anim) continue;
            for (unsigned c = 0; c < anim->mNumChannels; ++c) {
                const aiNodeAnim* channel = anim->mChannels[c];
                if (!channel) continue;
                const std::string nodeName = channel->mNodeName.C_Str();
                if (nodeToBoneIndex.find(nodeName) != nodeToBoneIndex.end()) continue;
                ::eyelib::animation::Bone newBone;
                newBone.name = nodeName;
                newBone.parentIndex = -1;
                newBone.inverseBindMatrix = math::Mat4(1.0f);
                newBone.bindPosition = math::Vec3(0.0f);
                newBone.bindRotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
                newBone.bindScale = math::Vec3(1.0f);
                const int newIndex = static_cast<int>(skeleton->BoneCount());
                skeleton->AddBone(newBone);
                nodeToBoneIndex[nodeName] = newIndex;
            }
        }
    }

    ProcessBoneHierarchy(scene->mRootNode, *skeleton, nodeToBoneIndex, -1);
    skeleton->BuildBoneMap();
    return skeleton;
}

std::vector<std::shared_ptr<::eyelib::animation::AnimationClip>> BuildClips(
    const aiScene* scene, const ::eyelib::animation::Skeleton& skeleton) {
    std::vector<std::shared_ptr<::eyelib::animation::AnimationClip>> clips;
    if (!scene) return clips;

    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* anim = scene->mAnimations[a];
        const double ticks = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
        const float duration = static_cast<float>(anim->mDuration / ticks);
        auto clip = std::make_shared<::eyelib::animation::AnimationClip>(anim->mName.C_Str(), duration);

        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* channel = anim->mChannels[c];
            const int boneIndex = skeleton.FindBoneIndex(channel->mNodeName.C_Str());
            if (boneIndex < 0) continue;

            for (unsigned i = 0; i < channel->mNumPositionKeys; ++i) {
                const auto& key = channel->mPositionKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                ::eyelib::animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    ::eyelib::animation::KeyframeEditor::Track::Position);
            }
            for (unsigned i = 0; i < channel->mNumRotationKeys; ++i) {
                const auto& key = channel->mRotationKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                ::eyelib::animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z));
            }
            for (unsigned i = 0; i < channel->mNumScalingKeys; ++i) {
                const auto& key = channel->mScalingKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                ::eyelib::animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    ::eyelib::animation::KeyframeEditor::Track::Scale);
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
                        const ::eyelib::animation::Skeleton& skeleton,
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

std::shared_ptr<::eyelib::render::Mesh> BuildMesh(const aiMesh* ai,
                                                  ::eyelib::animation::Skeleton* skeleton,
                                                  const math::Mat4& meshTransform) {
    auto mesh = std::make_shared<::eyelib::render::Mesh>();
    std::vector<math::IVec4> boneIndicesPerVertex;
    std::vector<math::Vec4> boneWeightsPerVertex;
    if (ai && ai->HasBones() && skeleton) {
        ExtractBoneWeights(ai, *skeleton, boneIndicesPerVertex, boneWeightsPerVertex);
    }

    std::vector<::eyelib::render::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.resize(ai->mNumVertices);

    const bool isSkinned = ai && ai->HasBones() && skeleton;
    const math::Mat4 appliedTransform = isSkinned ? math::Mat4(1.0f) : meshTransform;
    const math::Mat3 normalMatrix = glm::transpose(glm::inverse(math::Mat3(appliedTransform)));

    for (unsigned i = 0; i < ai->mNumVertices; ++i) {
        auto& v = vertices[i];
        math::Vec3 pos(ai->mVertices[i].x, ai->mVertices[i].y, ai->mVertices[i].z);
        v.position = math::Vec3(appliedTransform * math::Vec4(pos, 1.0f));
        if (ai->mNormals) {
            math::Vec3 n(ai->mNormals[i].x, ai->mNormals[i].y, ai->mNormals[i].z);
            v.normal = glm::normalize(normalMatrix * n);
        }
        if (ai->mTextureCoords[0]) {
            v.uv = math::Vec2(ai->mTextureCoords[0][i].x, ai->mTextureCoords[0][i].y);
        }
        if (ai->mTangents) {
            math::Vec3 t(ai->mTangents[i].x, ai->mTangents[i].y, ai->mTangents[i].z);
            v.tangent = glm::normalize(normalMatrix * t);
        }
        v.color = math::Vec4(1, 1, 1, 1);
        if (!boneIndicesPerVertex.empty() && i < boneIndicesPerVertex.size()) {
            v.boneIndices = boneIndicesPerVertex[i];
            v.boneWeights = boneWeightsPerVertex[i];
        } else {
            v.boneIndices = math::IVec4(0);
            v.boneWeights = math::Vec4(0.0f);
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
    mesh->AddSubmesh(0, static_cast<uint32_t>(indices.size()), static_cast<int>(ai->mMaterialIndex));
    mesh->UploadToGPU();
    return mesh;
}

std::shared_ptr<::eyelib::render::Texture2D> LoadAlbedoTexture(
    const aiScene* scene, unsigned materialIndex, const std::string& baseDir) {
    if (!scene || materialIndex >= scene->mNumMaterials) return nullptr;
    const aiMaterial* mat = scene->mMaterials[materialIndex];
    aiString path;
    if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
        mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == aiReturn_SUCCESS) {
        // ok
    } else if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
               mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == aiReturn_SUCCESS) {
        // ok
    } else {
        return nullptr;
    }

    std::string texPath = path.C_Str();
    if (!baseDir.empty()) {
        texPath = platform::filesystem::NormalizePath(baseDir + "/" + texPath);
    }
    auto tex = std::make_shared<::eyelib::render::Texture2D>();
    if (!tex->LoadFromFile(texPath, true, true)) {
        return nullptr;
    }
    return tex;
}

} // namespace

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& path) {
    Assimp::Importer importer;
    unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
    if (!IsGLTF(path)) {
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_JoinIdenticalVertices;
    }

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) {
        core::Log::WarnF("ModelLoader: failed to load %s", path.c_str());
        return nullptr;
    }

    auto skeleton = BuildSkeleton(scene);
    auto clips = BuildClips(scene, skeleton ? *skeleton : ::eyelib::animation::Skeleton{});
    auto clip = clips.empty() ? nullptr : clips.front();

    std::vector<std::shared_ptr<::eyelib::render::Mesh>> meshes;
    meshes.reserve(scene->mNumMeshes);
    std::vector<math::Mat4> meshTransforms(scene->mNumMeshes, math::Mat4(1.0f));
    CollectMeshTransforms(scene->mRootNode, math::Mat4(1.0f), meshTransforms);

    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const math::Mat4 meshTransform = (i < meshTransforms.size()) ? meshTransforms[i] : math::Mat4(1.0f);
        meshes.push_back(BuildMesh(scene->mMeshes[i], skeleton.get(), meshTransform));
    }

    const std::string baseDir = platform::filesystem::GetDirectory(path);
    std::shared_ptr<::eyelib::render::Shader> skinnedShader = std::make_shared<::eyelib::render::Shader>();
    skinnedShader->LoadFromFile("assets/shaders/passes/forward/skinned.vert",
                                "assets/shaders/passes/forward/skinned.frag");

    auto defaultAlbedo = std::make_shared<::eyelib::render::Texture2D>();
    {
        const uint8_t white[4] = {255, 255, 255, 255};
        defaultAlbedo->Create(1, 1, ::eyelib::render::TextureFormat::RGBA8, white);
    }

    std::vector<std::shared_ptr<::eyelib::render::Material>> materials;
    materials.resize(scene->mNumMaterials);
    for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
        auto mat = std::make_shared<::eyelib::render::Material>();
        mat->SetShader(skinnedShader);
        auto albedo = LoadAlbedoTexture(scene, i, baseDir);
        mat->SetAlbedoTexture(albedo ? albedo : defaultAlbedo);
        materials[i] = mat;
    }

    std::vector<std::shared_ptr<::eyelib::api::Mesh>> apiMeshes;
    apiMeshes.reserve(meshes.size());
    for (const auto& m : meshes) {
        apiMeshes.push_back(::eyelib::api::Mesh::WrapInternal(m));
    }

    std::vector<std::shared_ptr<::eyelib::api::Material>> apiMaterials;
    apiMaterials.reserve(materials.size());
    for (const auto& m : materials) {
        apiMaterials.push_back(::eyelib::api::Material::WrapInternal(m));
    }

    return ModelAsset::WrapInternal(std::move(apiMeshes), std::move(apiMaterials),
                                    Skeleton::WrapInternal(skeleton),
                                    AnimationClip::WrapInternal(clip));
}

} // namespace api
} // namespace eyelib
