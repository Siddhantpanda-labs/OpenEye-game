#include "MeshLoader.h"
#include "core/log/Log.h"
#include "animation/AnimationLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <filesystem>

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
    
    // Log embedded textures for debugging
    if (scene->mNumTextures > 0) {
        OE_INFO_F("Scene has %u embedded textures", scene->mNumTextures);
        for (unsigned int i = 0; i < scene->mNumTextures; i++) {
            if (scene->mTextures[i]) {
                OE_INFO_F("  Embedded texture %u: '%s' (size: %u, height: %u)", 
                         i, 
                         scene->mTextures[i]->mFilename.C_Str(),
                         scene->mTextures[i]->mWidth,
                         scene->mTextures[i]->mHeight);
            }
        }
    }
    
    std::vector<std::shared_ptr<LoadedMesh>> loadedMeshes;
    
    std::string basePath = path.substr(0, path.find_last_of("/\\") + 1);
    
    // Check if scene has bones (for skeletal animation)
    std::shared_ptr<Animation::Skeleton> skeleton = nullptr;
    std::vector<std::shared_ptr<Animation::AnimationClip>> animations;
    
    bool hasBones = false;
    if (scene->HasMeshes()) {
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            if (scene->mMeshes[i]->HasBones()) {
                hasBones = true;
                break;
            }
        }
    }
    
    // Extract skeleton and animations if model has bones
    if (hasBones && scene->mNumMeshes > 0) {
        skeleton = Animation::AnimationLoader::ExtractSkeleton(scene, 0);
        if (skeleton) {
            animations = Animation::AnimationLoader::ExtractAnimations(scene, *skeleton);
            OE_INFO_F("Extracted skeleton with %zu bones and %zu animation clips", 
                     skeleton->GetBoneCount(), animations.size());
        }
    }
    
    ProcessNode(scene->mRootNode, scene, loadedMeshes, basePath, skeleton);
    
    // Assign skeleton and animations to all loaded meshes (shared across meshes)
    for (auto& loadedMesh : loadedMeshes) {
        loadedMesh->skeleton = skeleton;
        loadedMesh->animations = animations;
    }
    
    OE_INFO_F("Loaded %zu meshes from: %s", loadedMeshes.size(), path.c_str());
    return loadedMeshes;
}

void MeshLoader::ProcessNode(aiNode* node, const aiScene* scene,
                             std::vector<std::shared_ptr<LoadedMesh>>& meshes,
                             const std::string& basePath,
                             std::shared_ptr<Animation::Skeleton> skeleton) {
    // Process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        auto processedMesh = ProcessMesh(mesh, scene, skeleton);
        
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
        ProcessNode(node->mChildren[i], scene, meshes, basePath, skeleton);
    }
}

std::shared_ptr<Mesh> MeshLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene, 
                                              std::shared_ptr<Animation::Skeleton> skeleton) {
    auto processedMesh = std::make_shared<Mesh>();
    
    // Extract bone weights first (in original Assimp vertex order) if skeleton exists
    std::vector<glm::ivec4> boneIndicesPerVertex;  // Per original vertex index
    std::vector<glm::vec4> boneWeightsPerVertex;    // Per original vertex index
    
    if (skeleton && mesh->HasBones()) {
        Animation::AnimationLoader::ExtractBoneWeights(mesh, *skeleton, 
                                                       boneIndicesPerVertex, 
                                                       boneWeightsPerVertex);
    }
    
    // Extract vertices using face-based indexing (preserves UV mapping)
    // This creates vertices in face order, not original vertex order
    std::vector<Vertex> vertices = ExtractVertices(mesh, skeleton);
    
    // Map bone weights from original vertex order to face-ordered vertices
    if (skeleton && mesh->HasBones() && !boneIndicesPerVertex.empty()) {
        // Bone weights were extracted per original vertex index
        // Now map them to face-ordered vertices by looking up the original vertex index from each face
        std::vector<glm::ivec4> mappedBoneIndices;
        std::vector<glm::vec4> mappedBoneWeights;
        mappedBoneIndices.reserve(vertices.size());
        mappedBoneWeights.reserve(vertices.size());
        
        for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
            const aiFace& face = mesh->mFaces[faceIdx];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                unsigned int originalVertexIndex = face.mIndices[j];
                if (originalVertexIndex < boneIndicesPerVertex.size()) {
                    mappedBoneIndices.push_back(boneIndicesPerVertex[originalVertexIndex]);
                    mappedBoneWeights.push_back(boneWeightsPerVertex[originalVertexIndex]);
                } else {
                    mappedBoneIndices.push_back(glm::ivec4(0));
                    mappedBoneWeights.push_back(glm::vec4(0.0f));
                }
            }
        }
        
        // Apply mapped bone data to vertices
        if (mappedBoneIndices.size() == vertices.size()) {
            for (size_t i = 0; i < vertices.size(); i++) {
                vertices[i].boneIndices = mappedBoneIndices[i];
                vertices[i].boneWeights = mappedBoneWeights[i];
            }
        }
    }
    
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

// Helper function to extract filename from path (handles both Windows and Unix paths)
static std::string GetFilename(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

// Helper function to load embedded texture from scene by name/index
static const aiTexture* FindEmbeddedTexture(const aiScene* scene, const std::string& texturePath) {
    if (!scene || scene->mNumTextures == 0) {
        return nullptr;
    }
    
    // First check if it's an index reference ("*0", "*1", etc.)
    if (texturePath.length() > 0 && texturePath[0] == '*') {
        try {
            unsigned int index = std::stoul(texturePath.substr(1));
            if (index < scene->mNumTextures) {
                return scene->mTextures[index];
            }
        } catch (...) {
            // Invalid index, continue to name matching
        }
    }
    
    // Try to find by filename (case-insensitive, path-normalized)
    std::string searchFilename = GetFilename(texturePath);
    std::transform(searchFilename.begin(), searchFilename.end(), searchFilename.begin(), ::tolower);
    
    for (unsigned int i = 0; i < scene->mNumTextures; i++) {
        if (scene->mTextures[i]) {
            std::string embeddedFilename = GetFilename(scene->mTextures[i]->mFilename.C_Str());
            std::transform(embeddedFilename.begin(), embeddedFilename.end(), embeddedFilename.begin(), ::tolower);
            
            if (embeddedFilename == searchFilename) {
                return scene->mTextures[i];
            }
        }
    }
    
    // Try Assimp's GetEmbeddedTexture helper (if available)
    #ifdef __cplusplus
    const aiTexture* embeddedByName = scene->GetEmbeddedTexture(texturePath.c_str());
    if (embeddedByName) {
        return embeddedByName;
    }
    #endif
    
    return nullptr;
}

// Helper function to load texture from Assimp material
static std::shared_ptr<Texture> LoadMaterialTexture(aiMaterial* material, aiTextureType type, const aiScene* scene, const std::string& basePath, bool isSRGB = true) {
    if (material->GetTextureCount(type) > 0) {
        aiString str;
        aiTextureMapMode mapMode[3];
        
        // Get texture path and mapping mode
        if (material->GetTexture(type, 0, &str, nullptr, nullptr, nullptr, nullptr, mapMode) == AI_SUCCESS) {
            std::string texturePath = std::string(str.C_Str());
            
            // First, try to find embedded texture (by index or by name)
            const aiTexture* embeddedTex = FindEmbeddedTexture(scene, texturePath);
            if (embeddedTex) {
                auto texture = std::make_shared<Texture>();
                if (embeddedTex->mHeight == 0) {
                    // Compressed data (e.g., PNG/JPG). mWidth is data size in bytes
                    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(embeddedTex->pcData);
                    int sizeBytes = static_cast<int>(embeddedTex->mWidth);
                    if (texture->LoadFromMemory(bytes, sizeBytes, isSRGB)) {
                        texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                        OE_INFO_F("Loaded embedded compressed texture: '%s' (%d bytes)", texturePath.c_str(), sizeBytes);
                        return texture;
                    } else {
                        OE_WARN_F("Failed to decode embedded compressed texture: '%s'", texturePath.c_str());
                    }
                } else {
                    // Uncompressed ARGB data (mWidth x mHeight)
                    int width = static_cast<int>(embeddedTex->mWidth);
                    int height = static_cast<int>(embeddedTex->mHeight);
                    const unsigned char* pixels = reinterpret_cast<const unsigned char*>(embeddedTex->pcData);
                    if (texture->LoadFromPixelsRGBA8(width, height, pixels, isSRGB)) {
                        texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                        OE_INFO_F("Loaded embedded raw texture: '%s' (%dx%d)", texturePath.c_str(), width, height);
                        return texture;
                    } else {
                        OE_WARN_F("Failed to upload embedded raw texture: '%s'", texturePath.c_str());
                    }
                }
            }
            
            // If not found as embedded, try external file paths
            // (Only if texturePath doesn't start with '*' - meaning it's not an embedded reference)
            if (texturePath.length() == 0 || texturePath[0] != '*') {
                // External texture file - try multiple path resolutions
                auto texture = std::make_shared<Texture>();
                
                // Try paths in order:
                // 1. Relative to base path (original path)
                std::string fullPath = basePath + texturePath;
                std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
                if (texture->LoadFromFile(fullPath, isSRGB)) {
                    texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                    OE_INFO_F("Loaded texture: %s", fullPath.c_str());
                    return texture;
                }
                
                // 2. Try just the filename in base path (sometimes paths are absolute or relative to model)
                std::filesystem::path pathObj(texturePath);
                std::string filename = pathObj.filename().string();
                std::string filenamePath = basePath + filename;
                std::replace(filenamePath.begin(), filenamePath.end(), '\\', '/');
                if (texture->LoadFromFile(filenamePath, isSRGB)) {
                    texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                    OE_INFO_F("Loaded texture (filename): %s", filenamePath.c_str());
                    return texture;
                }
                
                // 2b. Try searching in subdirectories common names (fbm, textures, maps, etc.)
                std::vector<std::string> subdirs = {"fbm", "textures", "maps", "tex", "images", "img"};
                for (const auto& subdir : subdirs) {
                    std::string subdirPath = basePath + subdir + "/" + filename;
                    std::replace(subdirPath.begin(), subdirPath.end(), '\\', '/');
                    if (texture->LoadFromFile(subdirPath, isSRGB)) {
                        texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                        OE_INFO_F("Loaded texture (subdirectory): %s", subdirPath.c_str());
                        return texture;
                    }
                }
                
                // 3. Try absolute path (if texturePath is already absolute)
                if (std::filesystem::exists(texturePath)) {
                    if (texture->LoadFromFile(texturePath, isSRGB)) {
                        texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
                        OE_INFO_F("Loaded texture (absolute): %s", texturePath.c_str());
                        return texture;
                    }
                }
                
                OE_WARN_F("Failed to load external texture: '%s' (tried: %s, %s, %s)", 
                         texturePath.c_str(), fullPath.c_str(), filenamePath.c_str(), texturePath.c_str());
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Material> MeshLoader::LoadMaterial(aiMaterial* material, const aiScene* scene, const std::string& basePath) {
    auto mat = std::make_shared<Material>();
    
    // Load Albedo/Diffuse texture (SRGB)
    if (auto albedoTexture = LoadMaterialTexture(material, aiTextureType_DIFFUSE, scene, basePath, true)) {
        mat->SetAlbedoTexture(albedoTexture);
    }
    // Also check BASE_COLOR (glTF/glTF2)
    if (!mat->GetTexture("u_AlbedoTexture")) {
        if (auto baseColorTexture = LoadMaterialTexture(material, aiTextureType_BASE_COLOR, scene, basePath, true)) {
            mat->SetAlbedoTexture(baseColorTexture);
        }
    }
    
    // Load Normal map (linear, not SRGB)
    if (auto normalTexture = LoadMaterialTexture(material, aiTextureType_NORMALS, scene, basePath, false)) {
        mat->SetNormalTexture(normalTexture);
    }
    // Also check HEIGHT (sometimes used as normal)
    if (!mat->GetTexture("u_NormalTexture")) {
        if (auto heightTexture = LoadMaterialTexture(material, aiTextureType_HEIGHT, scene, basePath, false)) {
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
                    if (auto metallicRoughTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), scene, basePath, false)) {
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
                        if (auto metallicTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), scene, basePath, false)) {
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
                        if (auto roughnessTexture = LoadMaterialTexture(material, static_cast<aiTextureType>(texType), scene, basePath, false)) {
                            mat->SetRoughnessTexture(roughnessTexture);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Load AO texture
    if (auto aoTexture = LoadMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION, scene, basePath, false)) {
        mat->SetAOTexture(aoTexture);
    }
    // Also check LIGHTMAP (sometimes used for AO)
    if (!mat->GetTexture("u_AOTexture")) {
        if (auto lightmapTexture = LoadMaterialTexture(material, aiTextureType_LIGHTMAP, scene, basePath, false)) {
            mat->SetAOTexture(lightmapTexture);
        }
    }
    
    // Load Emissive texture
    if (auto emissiveTexture = LoadMaterialTexture(material, aiTextureType_EMISSIVE, scene, basePath, true)) {
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

std::vector<Vertex> MeshLoader::ExtractVertices(aiMesh* mesh, 
                                                std::shared_ptr<Animation::Skeleton> skeleton) {
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

