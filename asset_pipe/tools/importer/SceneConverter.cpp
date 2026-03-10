#include "asset_pipe/tools/importer/SceneConverter.h"
#include "asset_pipe/tools/importer/TextureExtraction.h"
#include "asset_pipe/core/import_scene.h"
#include "asset_pipe/core/import_mesh.h"
#include "asset_pipe/core/import_material.h"
#include "asset_pipe/core/import_node.h"
#include "asset_pipe/core/math_types.h"

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

namespace asset_pipe {

namespace {

std::string SanitizeName(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        else if (c == ' ' || c == '.' || c == '/' || c == '\\') c = '_';
    }
    return s;
}

void ToMat4(const aiMatrix4x4& m, Mat4& out) {
    out.m[0] = m.a1; out.m[1] = m.a2; out.m[2] = m.a3; out.m[3] = m.a4;
    out.m[4] = m.b1; out.m[5] = m.b2; out.m[6] = m.b3; out.m[7] = m.b4;
    out.m[8] = m.c1; out.m[9] = m.c2; out.m[10] = m.c3; out.m[11] = m.c4;
    out.m[12] = m.d1; out.m[13] = m.d2; out.m[14] = m.d3; out.m[15] = m.d4;
}

void ExtractMeshes(const aiScene* scene, const std::string& sourceName, ImportScene& out) {
    if (!scene->HasMeshes()) return;
    out.meshes.reserve(scene->mNumMeshes);
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* aiMesh = scene->mMeshes[i];
        ImportMesh mesh;
        mesh.name = sourceName + "_" + SanitizeName(aiMesh->mName.C_Str());
        if (mesh.name.size() > 2 && mesh.name.substr(mesh.name.size() - 2) == "_") mesh.name.pop_back();

        const unsigned nv = aiMesh->mNumVertices;
        mesh.vertices.reserve(nv);
        float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
        float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;

        for (unsigned v = 0; v < nv; ++v) {
            ImportVertex vert;
            vert.position.x = aiMesh->mVertices[v].x;
            vert.position.y = aiMesh->mVertices[v].y;
            vert.position.z = aiMesh->mVertices[v].z;
            minX = std::min(minX, vert.position.x); maxX = std::max(maxX, vert.position.x);
            minY = std::min(minY, vert.position.y); maxY = std::max(maxY, vert.position.y);
            minZ = std::min(minZ, vert.position.z); maxZ = std::max(maxZ, vert.position.z);

            if (aiMesh->HasNormals()) {
                vert.normal.x = aiMesh->mNormals[v].x;
                vert.normal.y = aiMesh->mNormals[v].y;
                vert.normal.z = aiMesh->mNormals[v].z;
            } else {
                vert.normal.z = 1.f;
            }
            if (aiMesh->HasTextureCoords(0)) {
                vert.uv0.x = aiMesh->mTextureCoords[0][v].x;
                vert.uv0.y = aiMesh->mTextureCoords[0][v].y;
            }
            if (aiMesh->HasTangentsAndBitangents()) {
                vert.tangent.x = aiMesh->mTangents[v].x;
                vert.tangent.y = aiMesh->mTangents[v].y;
                vert.tangent.z = aiMesh->mTangents[v].z;
                vert.tangent.w = 1.f;
            } else {
                vert.tangent.w = 1.f;
            }
            mesh.vertices.push_back(vert);
        }

        mesh.bounds.min.x = minX; mesh.bounds.min.y = minY; mesh.bounds.min.z = minZ;
        mesh.bounds.max.x = maxX; mesh.bounds.max.y = maxY; mesh.bounds.max.z = maxZ;

        for (unsigned f = 0; f < aiMesh->mNumFaces; ++f) {
            const aiFace& face = aiMesh->mFaces[f];
            if (face.mNumIndices >= 3) {
                mesh.indices.push_back(face.mIndices[0]);
                mesh.indices.push_back(face.mIndices[1]);
                mesh.indices.push_back(face.mIndices[2]);
            }
        }

        if (aiMesh->mMaterialIndex < scene->mNumMaterials) {
            ImportSubmesh sub;
            sub.indexStart = 0;
            sub.indexCount = static_cast<uint32_t>(mesh.indices.size());
            sub.materialIndex = static_cast<uint32_t>(aiMesh->mMaterialIndex);
            mesh.submeshes.push_back(sub);
        }
        out.meshes.push_back(std::move(mesh));
    }
}

void ExtractMaterials(const aiScene* scene, const std::string& sourceName, ImportScene& out) {
    if (!scene->HasMaterials()) return;
    out.materials.reserve(scene->mNumMaterials);
    for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* mat = scene->mMaterials[i];
        ImportMaterial imp;
        aiString name;
        if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
            imp.name = sourceName + "_" + SanitizeName(name.C_Str());
        else
            imp.name = sourceName + "_mat" + std::to_string(i);

        aiString path;
        if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS ||
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            imp.albedo.path = path.C_Str();
        }
        if (mat->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS)
            imp.normal.path = path.C_Str();
        if (mat->GetTexture(aiTextureType_METALNESS, 0, &path) == AI_SUCCESS)
            imp.metallic.path = path.C_Str();
        if (mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path) == AI_SUCCESS)
            imp.roughness.path = path.C_Str();

        float f;
        if (mat->Get(AI_MATKEY_METALLIC_FACTOR, f) == AI_SUCCESS) imp.metallicValue = f;
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, f) == AI_SUCCESS) imp.roughnessValue = f;
        out.materials.push_back(std::move(imp));
    }
}

int AddNode(const aiNode* node, const aiScene* scene, ImportScene& out) {
    ImportNode n;
    n.name = node->mName.C_Str();
    ToMat4(node->mTransformation, n.localTransform);
    n.meshIndex = -1;
    if (node->mNumMeshes > 0) {
        n.meshIndex = static_cast<int>(node->mMeshes[0]);
    }
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        int childIdx = AddNode(node->mChildren[i], scene, out);
        n.children.push_back(childIdx);
    }
    int idx = static_cast<int>(out.nodes.size());
    out.nodes.push_back(std::move(n));
    return idx;
}

} // namespace

bool SceneConverter::Convert(const aiScene* scene, const char* sourcePath, ImportScene& out) {
    if (!scene) return false;

    out = ImportScene{};
    out.sourcePath = sourcePath ? sourcePath : "";
    std::filesystem::path p(out.sourcePath);
    out.sourceName = SanitizeName(p.stem().string());
    if (out.sourceName.empty()) out.sourceName = "imported";

    ExtractMeshes(scene, out.sourceName, out);
    ExtractMaterials(scene, out.sourceName, out);
    ExtractTextures(scene, sourcePath, out.sourceName, out);

    if (scene->mRootNode) {
        out.rootNodeIndex = AddNode(scene->mRootNode, scene, out);
    }

    return true;
}

} // namespace asset_pipe
