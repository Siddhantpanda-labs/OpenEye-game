#include "MeshLoader.h"
#include "core/log/Log.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <unordered_map>

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
    
    // For glTF files, don't flip UVs (OpenGL convention is already correct)
    // For other formats, may need aiProcess_FlipUVs
    bool isGLTF = path.length() >= 4 && 
                   (path.substr(path.length() - 5) == ".gltf" || 
                    path.substr(path.length() - 5) == ".glb" ||
                    path.find(".gltf") != std::string::npos ||
                    path.find(".glb") != std::string::npos);
    
    unsigned int flags = aiProcess_Triangulate |
                         aiProcess_GenNormals |
                         aiProcess_CalcTangentSpace;
    
    // For glTF: Don't flip UVs (already OpenGL convention), don't generate UVs (model has them)
    // Also don't join identical vertices for glTF - preserve vertex order and separate vertices
    // that might have different UVs but same positions (important for texture mapping)
    // For other formats: May need UV flipping and generation
    if (!isGLTF) {
        flags |= aiProcess_FlipUVs;
        flags |= aiProcess_GenUVCoords; // Generate UV coordinates if missing (only for non-glTF)
        flags |= aiProcess_JoinIdenticalVertices; // Only join for non-glTF
    }
    
    const aiScene* scene = importer.ReadFile(path, flags);
    
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
    
    // Extract vertices using face-based indexing (preserves UV mapping)
    std::vector<Vertex> vertices = ExtractVertices(mesh);
    // Generate sequential indices since ExtractVertices now creates one vertex per face index
    std::vector<uint32_t> indices(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++) {
        indices[i] = static_cast<uint32_t>(i);
    }
    
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

// Helper function to load texture from Assimp material
static std::shared_ptr<Texture> LoadMaterialTexture(aiMaterial* material, aiTextureType type, const std::string& basePath, bool isSRGB = true) {
    if (material->GetTextureCount(type) > 0) {
        aiString str;
        if (material->GetTexture(type, 0, &str) == AI_SUCCESS) {
            std::string texturePath = basePath + std::string(str.C_Str());
            // Normalize path separators
            std::replace(texturePath.begin(), texturePath.end(), '\\', '/');
            
            auto texture = std::make_shared<Texture>();
            if (texture->LoadFromFile(texturePath, isSRGB)) {
                // Ensure texture uses REPEAT wrap mode (standard for UV maps)
                texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                return texture;
            } else {
                OE_WARN_F("Failed to load texture: %s", texturePath.c_str());
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Material> MeshLoader::LoadMaterial(aiMaterial* material, const aiScene* scene, const std::string& basePath) {
    auto mat = std::make_shared<Material>();
    
    // Load Albedo/Diffuse texture (SRGB)
    if (auto albedoTexture = LoadMaterialTexture(material, aiTextureType_DIFFUSE, basePath, true)) {
        mat->SetAlbedoTexture(albedoTexture);
    }
    // Also check BASE_COLOR (glTF/glTF2)
    if (!mat->GetTexture("u_AlbedoTexture")) {
        if (auto baseColorTexture = LoadMaterialTexture(material, aiTextureType_BASE_COLOR, basePath, true)) {
            mat->SetAlbedoTexture(baseColorTexture);
        }
    }
    
    // Load Normal map (linear, not SRGB)
    if (auto normalTexture = LoadMaterialTexture(material, aiTextureType_NORMALS, basePath, false)) {
        mat->SetNormalTexture(normalTexture);
    }
    // Also check HEIGHT (sometimes used as normal)
    if (!mat->GetTexture("u_NormalTexture")) {
        if (auto heightTexture = LoadMaterialTexture(material, aiTextureType_HEIGHT, basePath, false)) {
            mat->SetNormalTexture(heightTexture);
        }
    }
    
    // Load Metallic-Roughness texture (combined, glTF format: B=metallic, G=roughness)
    // Check all texture types to find metallic-roughness by name pattern
    for (int texType = 1; texType < AI_TEXTURE_TYPE_MAX; ++texType) {
        if (material->GetTextureCount(static_cast<aiTextureType>(texType)) > 0) {
            aiString texPath;
            if (material->GetTexture(static_cast<aiTextureType>(texType), 0, &texPath) == AI_SUCCESS) {
                std::string pathStr = std::string(texPath.C_Str());
                std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
                
                // Check if this looks like a metallic-roughness texture
                if (pathStr.find("metallicroughness") != std::string::npos ||
                    (pathStr.find("metallic") != std::string::npos && pathStr.find("roughness") != std::string::npos)) {
                    if (auto metallicRoughTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), basePath, false)) {
                        mat->SetMetallicRoughnessTexture(metallicRoughTexture);
                        break;
                    }
                }
            }
        }
    }
    
    // Try to load separate metallic and roughness textures
    if (!mat->GetTexture("u_MetallicRoughnessTexture")) {
        // Check for separate metallic texture (newer Assimp versions)
        for (int texType = 1; texType < AI_TEXTURE_TYPE_MAX; ++texType) {
            if (material->GetTextureCount(static_cast<aiTextureType>(texType)) > 0) {
                aiString texPath;
                if (material->GetTexture(static_cast<aiTextureType>(texType), 0, &texPath) == AI_SUCCESS) {
                    std::string pathStr = std::string(texPath.C_Str());
                    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
                    
                    if (pathStr.find("metallic") != std::string::npos && pathStr.find("roughness") == std::string::npos) {
                        if (auto metallicTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), basePath, false)) {
                            mat->SetMetallicTexture(metallicTexture);
                            break;
                        }
                    }
                }
            }
        }
        
        // Check for separate roughness texture
        for (int texType = 1; texType < AI_TEXTURE_TYPE_MAX; ++texType) {
            if (material->GetTextureCount(static_cast<aiTextureType>(texType)) > 0) {
                aiString texPath;
                if (material->GetTexture(static_cast<aiTextureType>(texType), 0, &texPath) == AI_SUCCESS) {
                    std::string pathStr = std::string(texPath.C_Str());
                    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
                    
                    if (pathStr.find("roughness") != std::string::npos && pathStr.find("metallic") == std::string::npos) {
                        if (auto roughnessTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), basePath, false)) {
                            mat->SetRoughnessTexture(roughnessTexture);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Load AO texture
    if (auto aoTexture = LoadMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION, basePath, false)) {
        mat->SetAOTexture(aoTexture);
    }
    // Also check LIGHTMAP (sometimes used for AO)
    if (!mat->GetTexture("u_AOTexture")) {
        if (auto lightmapTexture = LoadMaterialTexture(material, aiTextureType_LIGHTMAP, basePath, false)) {
            mat->SetAOTexture(lightmapTexture);
        }
    }
    
    // Load Emissive texture
    if (auto emissiveTexture = LoadMaterialTexture(material, aiTextureType_EMISSIVE, basePath, true)) {
        mat->SetEmissiveTexture(emissiveTexture);
    }
    
    // Load material properties (fallback values if textures not available)
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
    
    aiColor3D emissiveColor;
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
        mat->SetEmission(glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
    }
    
    float ao = 1.0f;
    // AO factor is not standard in Assimp, but we can check
    mat->SetAO(ao);
    
    return mat;
}

std::vector<Vertex> MeshLoader::ExtractVertices(aiMesh* mesh) {
    // Extract vertices following face indices exactly
    // This preserves the exact relationship between position, normal, UV per vertex
    // Important for glTF models where same position can have different UVs at seams
    // This matches how the Vulkan engine handles indexed vertices
    std::vector<Vertex> vertices;
    
    // Reserve space (typically 3 vertices per face)
    vertices.reserve(mesh->mNumFaces * 3);
    
    // Process each face and create vertices based on face indices
    // This ensures each vertex attribute combination is preserved correctly
    for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
        const aiFace& face = mesh->mFaces[faceIdx];
        
        // Process each vertex index in this face
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            unsigned int index = face.mIndices[j];
            
            // Create vertex from the indexed data
            Vertex vertex;
            
            // Position
            vertex.position = glm::vec3(
                mesh->mVertices[index].x,
                mesh->mVertices[index].y,
                mesh->mVertices[index].z
            );
            
            // Normal
            if (mesh->mNormals) {
                vertex.normal = glm::vec3(
                    mesh->mNormals[index].x,
                    mesh->mNormals[index].y,
                    mesh->mNormals[index].z
                );
            }
            
            // Texture coordinates (use first set, TEXCOORD_0 in glTF)
            // Check if this UV channel exists and has 2 components
            if (mesh->mTextureCoords[0] && mesh->mNumUVComponents[0] >= 2) {
                vertex.texCoord = glm::vec2(
                    mesh->mTextureCoords[0][index].x,
                    mesh->mTextureCoords[0][index].y
                );
            } else if (mesh->mTextureCoords[0] && mesh->mNumUVComponents[0] == 1) {
                // Handle 1D texture coordinates (unlikely but possible)
                vertex.texCoord = glm::vec2(mesh->mTextureCoords[0][index].x, 0.0f);
            } else {
                // No texture coordinates - set default
                vertex.texCoord = glm::vec2(0.0f);
            }
            
            // Tangent
            if (mesh->mTangents) {
                vertex.tangent = glm::vec3(
                    mesh->mTangents[index].x,
                    mesh->mTangents[index].y,
                    mesh->mTangents[index].z
                );
            }
            
            // Bitangent
            if (mesh->mBitangents) {
                vertex.bitangent = glm::vec3(
                    mesh->mBitangents[index].x,
                    mesh->mBitangents[index].y,
                    mesh->mBitangents[index].z
                );
            }
            
            vertices.push_back(vertex);
        }
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

