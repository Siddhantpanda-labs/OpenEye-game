#include "eyelib/assets/loaders/MeshLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/job/ThreadPool.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>

namespace eyelib {
namespace assets {

namespace {

static bool IsGLTF(const std::string& path) {
    size_t n = path.size();
    if (n < 4) return false;
    std::string s = path;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 32;
    if (n >= 5 && s.substr(n - 5) == ".gltf") return true;
    if (n >= 4 && s.substr(n - 4) == ".glb") return true;
    return false;
}

static void AddBoneData(render::Vertex& v, int boneIndex, float weight) {
    for (int i = 0; i < 4; ++i) {
        if (v.boneWeights[i] == 0.0f) {
            v.boneIndices[i] = boneIndex;
            v.boneWeights[i] = weight;
            return;
        }
    }
    int minIndex = 0;
    float minWeight = v.boneWeights[0];
    for (int i = 1; i < 4; ++i) {
        if (v.boneWeights[i] < minWeight) {
            minWeight = v.boneWeights[i];
            minIndex = i;
        }
    }
    if (weight > minWeight) {
        v.boneIndices[minIndex] = boneIndex;
        v.boneWeights[minIndex] = weight;
    }
}

static void ProcessMesh(const aiMesh* ai, const aiScene* scene, std::vector<DecodedMeshData>& out, const std::string& nodeName) {
    (void)scene;
    DecodedMeshData d;
    d.name = nodeName.empty() ? ai->mName.C_Str() : nodeName;

    std::vector<render::Vertex>& v = d.vertices;
    v.resize(ai->mNumVertices);

    for (unsigned i = 0; i < ai->mNumVertices; ++i) {
        render::Vertex& vx = v[i];
        vx.position.x = ai->mVertices[i].x;
        vx.position.y = ai->mVertices[i].y;
        vx.position.z = ai->mVertices[i].z;
        if (ai->mNormals) {
            vx.normal.x = ai->mNormals[i].x;
            vx.normal.y = ai->mNormals[i].y;
            vx.normal.z = ai->mNormals[i].z;
        } else {
            vx.normal = math::Vec3(0, 1, 0);
        }
        if (ai->mTextureCoords[0]) {
            vx.uv.x = ai->mTextureCoords[0][i].x;
            vx.uv.y = ai->mTextureCoords[0][i].y;
        } else {
            vx.uv = math::Vec2(0, 0);
        }
        if (ai->mTangents) {
            vx.tangent.x = ai->mTangents[i].x;
            vx.tangent.y = ai->mTangents[i].y;
            vx.tangent.z = ai->mTangents[i].z;
        }
        if (ai->mColors[0]) {
            vx.color.r = ai->mColors[0][i].r;
            vx.color.g = ai->mColors[0][i].g;
            vx.color.b = ai->mColors[0][i].b;
            vx.color.a = ai->mColors[0][i].a;
        } else {
            vx.color = math::Vec4(1, 1, 1, 1);
        }
        vx.boneIndices = math::IVec4(0);
        vx.boneWeights = math::Vec4(0.0f);
    }

    if (ai->HasBones()) {
        for (unsigned b = 0; b < ai->mNumBones; ++b) {
            const aiBone* bone = ai->mBones[b];
            const int boneIndex = static_cast<int>(b);
            for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                const aiVertexWeight& vw = bone->mWeights[w];
                if (vw.mVertexId >= v.size()) continue;
                AddBoneData(v[vw.mVertexId], boneIndex, vw.mWeight);
            }
        }
        for (auto& vx : v) {
            float sum = vx.boneWeights.x + vx.boneWeights.y + vx.boneWeights.z + vx.boneWeights.w;
            if (sum > 0.0f) vx.boneWeights /= sum;
        }
    }

    for (unsigned i = 0; i < ai->mNumFaces; ++i) {
        const aiFace& f = ai->mFaces[i];
        for (unsigned j = 0; j < f.mNumIndices; ++j)
            d.indices.push_back(f.mIndices[j]);
    }

    render::Submesh sub;
    sub.indexOffset = 0;
    sub.indexCount = static_cast<uint32_t>(d.indices.size());
    sub.materialIndex = static_cast<int>(ai->mMaterialIndex);
    d.submeshes.push_back(sub);

    out.push_back(std::move(d));
}

static void ProcessNode(aiNode* node, const aiScene* scene, std::vector<DecodedMeshData>& out, const std::string& baseName) {
    std::string name = baseName.empty() ? node->mName.C_Str() : baseName;
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* m = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(m, scene, out, name);
    }
    for (unsigned i = 0; i < node->mNumChildren; ++i)
        ProcessNode(node->mChildren[i], scene, out, name);
}

static std::vector<DecodedMeshData> ImportToDecoded(const aiScene* scene) {
    std::vector<DecodedMeshData> out;
    if (!scene || !scene->mRootNode) return out;
    ProcessNode(scene->mRootNode, scene, out, "");
    return out;
}

} // namespace

std::vector<std::shared_ptr<render::Mesh>> MeshLoader::LoadFromFile(const std::string& path) {
    Assimp::Importer imp;
    unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
    if (!IsGLTF(path)) {
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_JoinIdenticalVertices;
    }

    const aiScene* scene = imp.ReadFile(path, flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        core::Log::ErrorF("MeshLoader: %s - %s", path.c_str(), imp.GetErrorString());
        return {};
    }

    std::vector<DecodedMeshData> decoded = ImportToDecoded(scene);
    std::vector<std::shared_ptr<render::Mesh>> result;
    result.reserve(decoded.size());
    for (auto& d : decoded)
        result.push_back(CreateFromDecoded(d));
    return result;
}

std::shared_ptr<render::Mesh> MeshLoader::LoadSingle(const std::string& path) {
    auto vec = LoadFromFile(path);
    return vec.empty() ? nullptr : vec[0];
}

void MeshLoader::LoadAsync(const std::string& path, std::function<void(std::vector<DecodedMeshData>)> callback) {
    if (!callback) return;
    static core::job::ThreadPool pool(0);
    pool.Submit([path, callback]() {
        std::vector<uint8_t> bytes = platform::filesystem::ReadFile(path);
        if (bytes.empty()) {
            callback({});
            return;
        }
        Assimp::Importer imp;
        unsigned flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
        if (!IsGLTF(path)) {
            flags |= aiProcess_FlipUVs;
            flags |= aiProcess_JoinIdenticalVertices;
        }
        const aiScene* scene = imp.ReadFileFromMemory(bytes.data(), bytes.size(), flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            callback({});
            return;
        }
        callback(ImportToDecoded(scene));
    });
}

std::shared_ptr<render::Mesh> MeshLoader::CreateFromDecoded(const DecodedMeshData& data) {
    auto m = std::make_shared<render::Mesh>();
    m->SetVertices(data.vertices);
    m->SetIndices(data.indices);
    for (const auto& s : data.submeshes)
        m->AddSubmesh(s.indexOffset, s.indexCount, s.materialIndex);
    m->UploadToGPU();
    return m;
}

AssetId MeshLoader::LoadMesh(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Mesh");
    auto mesh = LoadSingle(path);
    if (mesh) {
        mesh->SetAssetId(id);
        manager.Cache().Store(id, mesh, "Mesh");
    }
    return id;
}

void MeshLoader::LoadMeshAsync(const std::string& path, LoadCallback callback) {
    std::thread([path, callback]() {
        const AssetId id = LoadMesh(path);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib
