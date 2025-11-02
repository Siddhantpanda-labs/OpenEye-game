#include "MeshLoader.h"
#include "core/log/Log.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cmath>

namespace OpenEye {
namespace Rendering {

std::shared_ptr<LoadedMesh> MeshLoader::LoadFromFile(const std::string& path) {
    auto meshes = LoadSceneFromFile(path);
    if (meshes.empty()) {
        return nullptr;
    }
    return meshes[0];
}

std::vector<std::shared_ptr<LoadedMesh>> MeshLoader::LoadSceneFromFile(const std::string& path) {
    Assimp::Importer importer;
    
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        OE_ERROR_F("Failed to load model: %s - %s", path.c_str(), importer.GetErrorString());
        return {};
    }
    
    std::vector<std::shared_ptr<LoadedMesh>> loadedMeshes;
    
    std::string basePath = path.substr(0, path.find_last_of("/\\") + 1);
    
    ProcessNode(scene->mRootNode, scene, loadedMeshes, basePath);
    
    OE_INFO_F("Loaded %zu meshes from: %s", loadedMeshes.size(), path.c_str());
    return loadedMeshes;
}

void MeshLoader::ProcessNode(aiNode* node, const aiScene* scene,
                             std::vector<std::shared_ptr<LoadedMesh>>& meshes,
                             const std::string& basePath) {
    // Process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        auto processedMesh = ProcessMesh(mesh, scene);
        
        auto loadedMesh = std::make_shared<LoadedMesh>();
        loadedMesh->mesh = processedMesh;
        loadedMesh->name = std::string(node->mName.C_Str());
        
        // Load materials
        if (scene->mMaterials) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            loadedMesh->materials.push_back(LoadMaterial(material, scene, basePath));
        } else {
            // Default material
            loadedMesh->materials.push_back(std::make_shared<Material>());
        }
        
        meshes.push_back(loadedMesh);
    }
    
    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, meshes, basePath);
    }
}

std::shared_ptr<Mesh> MeshLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
    auto processedMesh = std::make_shared<Mesh>();
    
    std::vector<Vertex> vertices = ExtractVertices(mesh);
    std::vector<uint32_t> indices = ExtractIndices(mesh);
    
    processedMesh->SetVertices(vertices);
    processedMesh->SetIndices(indices);
    
    // Create submesh
    Submesh submesh;
    submesh.indexOffset = 0;
    submesh.indexCount = static_cast<uint32_t>(indices.size());
    submesh.vertexOffset = 0;
    submesh.vertexCount = static_cast<uint32_t>(vertices.size());
    submesh.materialIndex = mesh->mMaterialIndex;
    processedMesh->AddSubmesh(submesh);
    
    processedMesh->UploadToGPU();
    
    return processedMesh;
}

std::shared_ptr<Material> MeshLoader::LoadMaterial(aiMaterial* material, const aiScene* scene, const std::string& basePath) {
    auto mat = std::make_shared<Material>();
    
    // Load textures
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString str;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
        std::string texturePath = basePath + std::string(str.C_Str());
        
        auto texture = std::make_shared<Texture>();
        if (texture->LoadFromFile(texturePath, true)) {
            mat->SetTexture("u_AlbedoTexture", texture);
        }
    }
    
    // Load material properties
    aiColor3D color;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        mat->SetAlbedo(glm::vec3(color.r, color.g, color.b));
    }
    
    float metallic = 0.0f;
    if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        mat->SetMetallic(metallic);
    }
    
    float roughness = 0.5f;
    if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        mat->SetRoughness(roughness);
    }
    
    return mat;
}

std::vector<Vertex> MeshLoader::ExtractVertices(aiMesh* mesh) {
    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
    
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        
        // Position
        vertex.position = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );
        
        // Normal
        if (mesh->mNormals) {
            vertex.normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        }
        
        // Texture coordinates (use first set)
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        }
        
        // Tangent
        if (mesh->mTangents) {
            vertex.tangent = glm::vec3(
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
            );
        }
        
        // Bitangent
        if (mesh->mBitangents) {
            vertex.bitangent = glm::vec3(
                mesh->mBitangents[i].x,
                mesh->mBitangents[i].y,
                mesh->mBitangents[i].z
            );
        }
        
        vertices.push_back(vertex);
    }
    
    return vertices;
}

std::vector<uint32_t> MeshLoader::ExtractIndices(aiMesh* mesh) {
    std::vector<uint32_t> indices;
    
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    return indices;
}

std::shared_ptr<Mesh> MeshLoader::CreateCube() {
    auto mesh = std::make_shared<Mesh>();
    
    // 24 unique vertices (each face has its own vertex normals/tangents)
    std::vector<Vertex> vertices = {
        // Front (+Z)
        {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 0}, {1, 0, 0}, {0, 1, 0}},
        {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 0}, {1, 0, 0}, {0, 1, 0}},
        {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 1}, {1, 0, 0}, {0, 1, 0}},
        {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 1}, {1, 0, 0}, {0, 1, 0}},
        // Back (-Z)
        {{ 0.5f, -0.5f, -0.5f}, {0, 0,-1}, {0, 0}, {-1, 0, 0}, {0, 1, 0}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0,-1}, {1, 0}, {-1, 0, 0}, {0, 1, 0}},
        {{-0.5f,  0.5f, -0.5f}, {0, 0,-1}, {1, 1}, {-1, 0, 0}, {0, 1, 0}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 0,-1}, {0, 1}, {-1, 0, 0}, {0, 1, 0}},
        // Left (-X)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}, {0, 0, 1}, {0, 1, 0}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}, {0, 0, 1}, {0, 1, 0}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, {0, 0, 1}, {0, 1, 0}},
        // Right (+X)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {0, 0}, {0, 0,-1}, {0, 1, 0}},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1, 0}, {0, 0,-1}, {0, 1, 0}},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1, 1}, {0, 0,-1}, {0, 1, 0}},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {0, 1}, {0, 0,-1}, {0, 1, 0}},
        // Bottom (-Y)
        {{-0.5f, -0.5f, -0.5f}, {0,-1, 0}, {0, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ 0.5f, -0.5f, -0.5f}, {0,-1, 0}, {1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ 0.5f, -0.5f,  0.5f}, {0,-1, 0}, {1, 1}, {1, 0, 0}, {0, 0, 1}},
        {{-0.5f, -0.5f,  0.5f}, {0,-1, 0}, {0, 1}, {1, 0, 0}, {0, 0, 1}},
        // Top (+Y)
        {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0}, {1, 0, 0}, {0, 0,-1}},
        {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 0}, {1, 0, 0}, {0, 0,-1}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 1}, {1, 0, 0}, {0, 0,-1}},
        {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 1}, {1, 0, 0}, {0, 0,-1}},
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2,  2, 3, 0,      // Front
        4, 5, 6,  6, 7, 4,      // Back
        8, 9,10, 10,11, 8,      // Left
        12,13,14, 14,15,12,     // Right
        16,17,18, 18,19,16,     // Bottom
        20,21,22, 22,23,20      // Top
    };
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    
    return mesh;
}

std::shared_ptr<Mesh> MeshLoader::CreateSphere(int segments) {
    auto mesh = std::make_shared<Mesh>();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    for (int y = 0; y <= segments; y++) {
        for (int x = 0; x <= segments; x++) {
            float xSegment = static_cast<float>(x) / static_cast<float>(segments);
            float ySegment = static_cast<float>(y) / static_cast<float>(segments);
            
            float xPos = std::cos(xSegment * 2.0f * 3.14159f) * std::sin(ySegment * 3.14159f);
            float yPos = std::cos(ySegment * 3.14159f);
            float zPos = std::sin(xSegment * 2.0f * 3.14159f) * std::sin(ySegment * 3.14159f);
            
            Vertex vertex;
            vertex.position = glm::vec3(xPos, yPos, zPos) * 0.5f;
            vertex.normal = glm::normalize(glm::vec3(xPos, yPos, zPos));
            vertex.texCoord = glm::vec2(xSegment, ySegment);
            
            vertices.push_back(vertex);
        }
    }
    
    for (int y = 0; y < segments; y++) {
        for (int x = 0; x < segments; x++) {
            int first = y * (segments + 1) + x;
            int second = first + segments + 1;
            
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    
    return mesh;
}

std::shared_ptr<Mesh> MeshLoader::CreatePlane(float width, float height) {
    auto mesh = std::make_shared<Mesh>();
    
    float w = width * 0.5f;
    float h = height * 0.5f;
    
    std::vector<Vertex> vertices = {
        {{-w, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ w, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ w, 0.0f,  h}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-w, 0.0f,  h}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2,  2, 3, 0
    };
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    
    return mesh;
}

std::shared_ptr<Mesh> MeshLoader::CreateQuad() {
    return CreatePlane(2.0f, 2.0f);
}

} // namespace Rendering
} // namespace OpenEye

