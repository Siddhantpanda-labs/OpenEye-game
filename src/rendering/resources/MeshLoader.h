#pragma once

#include <string>
#include <memory>
#include <vector>
#include <assimp/scene.h>
#include "Mesh.h"
#include "Material.h"

namespace OpenEye {
namespace Rendering {

struct LoadedMesh {
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    std::string name;
};

class MeshLoader {
public:
    // Load mesh from file (using Assimp)
    static std::shared_ptr<LoadedMesh> LoadFromFile(const std::string& path);
    
    // Load multiple meshes from file (scene with multiple meshes)
    static std::vector<std::shared_ptr<LoadedMesh>> LoadSceneFromFile(const std::string& path);
    
    // Create simple primitive meshes
    static std::shared_ptr<Mesh> CreateCube();
    static std::shared_ptr<Mesh> CreateSphere(int segments = 32);
    static std::shared_ptr<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f);
    static std::shared_ptr<Mesh> CreateQuad();

private:
    static void ProcessNode(aiNode* node, const aiScene* scene, 
                           std::vector<std::shared_ptr<LoadedMesh>>& meshes,
                           const std::string& basePath);
    static std::shared_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
    static std::shared_ptr<Material> LoadMaterial(aiMaterial* material, const aiScene* scene, const std::string& basePath);
    static std::vector<Vertex> ExtractVertices(aiMesh* mesh);
    static std::vector<uint32_t> ExtractIndices(aiMesh* mesh);
};

} // namespace Rendering
} // namespace OpenEye

