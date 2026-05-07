// Standalone harness: EyelibRendering + GLFW + Vulkan device (no api/render, no EyelibRender).
#include "rendering/RenderingModule.h"

#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/RenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/materials/MaterialSchema.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb/stb_image_write.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

eyelib::rendering::MeshDescriptor MakeCenterCubeMeshDescriptor() {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    const float h = 0.6f;
    d.vertices.reserve(24);
    d.indices32.reserve(36);

    auto addFace = [&d](const eyelib::math::Vec3& n,
                        const eyelib::math::Vec3& v0,
                        const eyelib::math::Vec3& v1,
                        const eyelib::math::Vec3& v2,
                        const eyelib::math::Vec3& v3) {
        const uint32_t base = static_cast<uint32_t>(d.vertices.size());
        Vertex a{}; a.position = v0; a.normal = n; a.uv = {0.0f, 0.0f};
        Vertex b{}; b.position = v1; b.normal = n; b.uv = {1.0f, 0.0f};
        Vertex c{}; c.position = v2; c.normal = n; c.uv = {1.0f, 1.0f};
        Vertex d0{}; d0.position = v3; d0.normal = n; d0.uv = {0.0f, 1.0f};
        d.vertices.push_back(a);
        d.vertices.push_back(b);
        d.vertices.push_back(c);
        d.vertices.push_back(d0);
        d.indices32.push_back(base + 0u);
        d.indices32.push_back(base + 1u);
        d.indices32.push_back(base + 2u);
        d.indices32.push_back(base + 2u);
        d.indices32.push_back(base + 3u);
        d.indices32.push_back(base + 0u);
    };

    addFace({0.0f, 0.0f, 1.0f}, {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}); // +Z
    addFace({0.0f, 0.0f,-1.0f}, { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}); // -Z
    addFace({1.0f, 0.0f, 0.0f}, { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h}); // +X
    addFace({-1.0f,0.0f, 0.0f}, {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h}); // -X
    addFace({0.0f, 1.0f, 0.0f}, {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h}); // +Y
    addFace({0.0f,-1.0f, 0.0f}, {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}); // -Y

    d.submeshes.push_back(Submesh{0u, static_cast<uint32_t>(d.indices32.size()), 0});
    return d;
}

struct TextureSource {
    std::string filePath;
    std::vector<uint8_t> encodedBytes;
    bool valid = false;
};

struct ImportedMaterialData {
    eyelib::rendering::materials::MaterialRecord record{};
    TextureSource baseColor;
    TextureSource normal;
    TextureSource metallicRoughness;
    TextureSource emissive;
    TextureSource occlusion;
};

struct ImportedSceneData {
    eyelib::rendering::MeshDescriptor mesh{};
    std::vector<ImportedMaterialData> materials;
    uint32_t maxMaterialId = 0u;
};

bool ResolveMaterialTextureSource(const aiScene* scene, const std::filesystem::path& modelDir,
                                  const aiMaterial* material, aiTextureType textureType, TextureSource& out);

bool EncodeRgbaToPngBytes(const uint8_t* rgba, uint32_t width, uint32_t height, std::vector<uint8_t>& outBytes) {
    if (!rgba || width == 0u || height == 0u)
        return false;
    outBytes.clear();
    auto writeFn = [](void* context, void* data, int size) {
        auto* dst = static_cast<std::vector<uint8_t>*>(context);
        const auto* src = static_cast<const uint8_t*>(data);
        dst->insert(dst->end(), src, src + size);
    };
    const int stride = static_cast<int>(width) * 4;
    return stbi_write_png_to_func(writeFn, &outBytes, static_cast<int>(width), static_cast<int>(height), 4, rgba, stride) != 0;
}

bool TryEncodeEmbeddedTextureToBytes(const aiTexture* embedded, std::vector<uint8_t>& outBytes) {
    if (!embedded)
        return false;
    // Assimp embedded compressed payload (PNG/JPG/etc) - decodeable by stb_image.
    if (embedded->mHeight == 0 && embedded->pcData && embedded->mWidth > 0u) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(embedded->pcData);
        outBytes.assign(src, src + embedded->mWidth);
        return !outBytes.empty();
    }
    // Assimp embedded uncompressed payload (aiTexel RGBA) - encode to PNG for LoadFromMemory().
    if (embedded->mHeight > 0 && embedded->pcData && embedded->mWidth > 0u) {
        const uint32_t w = embedded->mWidth;
        const uint32_t h = embedded->mHeight;
        const size_t texelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        std::vector<uint8_t> rgba(texelCount * 4u);
        for (size_t i = 0; i < texelCount; ++i) {
            const aiTexel& t = embedded->pcData[i];
            rgba[i * 4u + 0u] = t.r;
            rgba[i * 4u + 1u] = t.g;
            rgba[i * 4u + 2u] = t.b;
            rgba[i * 4u + 3u] = t.a;
        }
        return EncodeRgbaToPngBytes(rgba.data(), w, h, outBytes);
    }
    return false;
}

bool TryGetEmbeddedTextureEncodedBytes(const aiScene* scene, const aiString& ref, std::vector<uint8_t>& outBytes) {
    if (!scene)
        return false;
    const char* s = ref.C_Str();
    if (!s || s[0] == '\0')
        return false;

    // Handle explicit *index references.
    if (s[0] == '*') {
        const int idx = std::atoi(s + 1);
        if (idx >= 0 && static_cast<unsigned>(idx) < scene->mNumTextures) {
            if (TryEncodeEmbeddedTextureToBytes(scene->mTextures[idx], outBytes))
                return true;
        }
    }

    // Handle named/URI-style embedded refs (common in Assimp glTF paths).
    if (const aiTexture* embedded = scene->GetEmbeddedTexture(s))
        return TryEncodeEmbeddedTextureToBytes(embedded, outBytes);

    return false;
}

bool ResolveMaterialTextureSourceMulti(const aiScene* scene, const std::filesystem::path& modelDir,
                                       const aiMaterial* material, const std::initializer_list<aiTextureType>& types,
                                       TextureSource& out) {
    for (aiTextureType t : types) {
        if (ResolveMaterialTextureSource(scene, modelDir, material, t, out))
            return true;
    }
    return false;
}

bool ResolveMaterialTextureSource(const aiScene* scene, const std::filesystem::path& modelDir,
                                  const aiMaterial* material, aiTextureType textureType, TextureSource& out) {
    if (!material || material->GetTextureCount(textureType) == 0u)
        return false;
    aiString texRef;
    if (material->GetTexture(textureType, 0, &texRef) != aiReturn_SUCCESS)
        return false;

    std::vector<uint8_t> embeddedBytes;
    if (TryGetEmbeddedTextureEncodedBytes(scene, texRef, embeddedBytes)) {
        out.encodedBytes = std::move(embeddedBytes);
        out.valid = true;
        return true;
    }

    const std::filesystem::path rel(texRef.C_Str());
    const std::filesystem::path abs = (modelDir / rel).lexically_normal();
    if (std::filesystem::exists(abs)) {
        out.filePath = abs.string();
        out.valid = true;
        return true;
    }
    return false;
}

eyelib::rendering::materials::MaterialRecord ParseAssimpMaterialFactors(const aiMaterial* material) {
    using namespace eyelib::rendering::materials;
    MaterialRecord out = MakeDefaultMaterialRecord();
    if (!material)
        return out;

    aiColor4D baseColor{1.f, 1.f, 1.f, 1.f};
    if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) != aiReturn_SUCCESS) {
        aiColor3D diffuse{1.f, 1.f, 1.f};
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == aiReturn_SUCCESS) {
            baseColor = aiColor4D(diffuse.r, diffuse.g, diffuse.b, 1.f);
        }
    }
    out.baseColorFactor = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};

    aiColor3D emissive{0.f, 0.f, 0.f};
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == aiReturn_SUCCESS) {
        out.emissiveFactor = {emissive.r, emissive.g, emissive.b, 1.f};
    }

    float metallic = out.metallicFactor;
    if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS)
        out.metallicFactor = metallic;
    float roughness = out.roughnessFactor;
    if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS)
        out.roughnessFactor = roughness;
    return out;
}

ImportedSceneData LoadSceneWithAssimp(const char* path) {
    using namespace eyelib::rendering;
    ImportedSceneData out{};
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
        
    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0u) {
        std::fprintf(stderr, "Assimp failed to load '%s': %s\n",
                     path ? path : "(null)",
                     importer.GetErrorString());
        out.mesh = MakeCenterCubeMeshDescriptor(); // Fallback
        return out;
    }

    const std::filesystem::path modelDir = std::filesystem::path(path).parent_path();
    out.materials.resize(scene->mNumMaterials);
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
        const aiMaterial* aiMat = scene->mMaterials[m];
        ImportedMaterialData material{};
        material.record = ParseAssimpMaterialFactors(aiMat);
        ResolveMaterialTextureSourceMulti(
            scene, modelDir, aiMat,
            {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE, aiTextureType_UNKNOWN},
            material.baseColor);
        ResolveMaterialTextureSourceMulti(
            scene, modelDir, aiMat,
            {aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT},
            material.normal);
        ResolveMaterialTextureSourceMulti(
            scene, modelDir, aiMat,
            {aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SPECULAR, aiTextureType_UNKNOWN},
            material.metallicRoughness);
        ResolveMaterialTextureSourceMulti(
            scene, modelDir, aiMat,
            {aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR},
            material.emissive);
        ResolveMaterialTextureSourceMulti(
            scene, modelDir, aiMat,
            {aiTextureType_LIGHTMAP, aiTextureType_AMBIENT_OCCLUSION, aiTextureType_AMBIENT},
            material.occlusion);
        out.materials[m] = std::move(material);
    }
    
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* ai_mesh = scene->mMeshes[i];
        Submesh sm{};
        sm.indexOffset = static_cast<uint32_t>(out.mesh.indices32.size());
        sm.indexCount = ai_mesh->mNumFaces * 3;
        sm.materialIndex = ai_mesh->mMaterialIndex;
        if (sm.materialIndex > out.maxMaterialId)
            out.maxMaterialId = sm.materialIndex;
        
        uint32_t vertexBase = static_cast<uint32_t>(out.mesh.vertices.size());
        
        for (unsigned int v = 0; v < ai_mesh->mNumVertices; v++) {
            Vertex vert{};
            vert.position = { ai_mesh->mVertices[v].x, ai_mesh->mVertices[v].y, ai_mesh->mVertices[v].z };
            if (ai_mesh->mNormals)
                vert.normal = { ai_mesh->mNormals[v].x, ai_mesh->mNormals[v].y, ai_mesh->mNormals[v].z };
            if (ai_mesh->mTextureCoords[0])
                vert.uv = { ai_mesh->mTextureCoords[0][v].x, ai_mesh->mTextureCoords[0][v].y };
            out.mesh.vertices.push_back(vert);
        }
        
        for (unsigned int f = 0; f < ai_mesh->mNumFaces; f++) {
            aiFace face = ai_mesh->mFaces[f];
            for (unsigned int ind = 0; ind < face.mNumIndices; ind++) {
                out.mesh.indices32.push_back(vertexBase + face.mIndices[ind]);
            }
        }
        
        out.mesh.submeshes.push_back(sm);
    }
    
    if (out.mesh.vertices.empty() || out.mesh.indices32.empty()) {
        std::fprintf(stderr, "Assimp loaded '%s' but produced no triangles; using fallback cube.\n", path);
        out.mesh = MakeCenterCubeMeshDescriptor();
        out.materials.clear();
        out.maxMaterialId = 0u;
        return out;
    }

    std::printf("Sandbox: loaded mesh '%s' (%u meshes, %u vertices, %u indices, %u submeshes)\n",
                path,
                static_cast<unsigned>(scene->mNumMeshes),
                static_cast<unsigned>(out.mesh.vertices.size()),
                static_cast<unsigned>(out.mesh.indices32.size()),
                static_cast<unsigned>(out.mesh.submeshes.size()));
    return out;
}

std::string ResolveSponzaPath() {
    namespace fs = std::filesystem;
    const std::array<const char*, 6> candidates = {
        "assets/models/sponza.glb",
        "Engine/assets/models/sponza.glb",
        "../assets/models/sponza.glb",
        "../Engine/assets/models/sponza.glb",
        "../../assets/models/sponza.glb",
        "../../Engine/assets/models/sponza.glb",
    };

    for (const char* rel : candidates) {
        const fs::path p(rel);
        if (fs::exists(p)) {
            return p.lexically_normal().string();
        }
    }
    return {};
}

struct SandboxMeshResolver final : eyelib::rendering::IMeshHandleResolver {
    eyelib::rendering::Mesh* mesh = nullptr;

    eyelib::rendering::MeshHandle TryResolveMesh(uint32_t meshHandle) const noexcept override {
        if (meshHandle == 0u && mesh)
            return mesh->GetHandle();
        return {};
    }
};

}  // namespace

int main() {
    std::printf("%s\n", eyelib::rendering::DescribeModule());

    if (!glfwInit()) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(kWidth), static_cast<int>(kHeight), "OpenEyeRenderingSandbox", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: glfwCreateWindow failed\n");
        glfwTerminate();
        return 2;
    }

    std::unique_ptr<eyelib::rendering::rhi::IDevice> device = eyelib::rendering::rhi::CreateVulkanDevice(window, kWidth, kHeight);
    if (!device) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: CreateVulkanDevice failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }

    ImportedSceneData imported{};
    imported.mesh = MakeCenterCubeMeshDescriptor();
    const std::string sponzaPath = ResolveSponzaPath();
    if (!sponzaPath.empty()) {
        imported = LoadSceneWithAssimp(sponzaPath.c_str());
    } else {
        std::fprintf(stderr,
                     "Sandbox: could not find sponza.glb under assets/models; using fallback cube.\n");
    }

    std::shared_ptr<eyelib::rendering::Mesh> sandboxMesh = eyelib::rendering::Mesh::Create(device.get(), imported.mesh);
    if (!sandboxMesh) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: Mesh::Create failed\n");
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    SandboxMeshResolver meshResolver{};
    meshResolver.mesh = sandboxMesh.get();

    eyelib::rendering::RasterFeature rasterFeature;
    eyelib::rendering::ShadowFeature shadowFeature;
    rasterFeature.SetMeshResolver(&meshResolver);
    shadowFeature.SetMeshResolver(&meshResolver);
    rasterFeature.SetRenderMode(eyelib::rendering::RenderMode::Deferred);
    eyelib::rendering::RenderSceneView sceneView;

    {
        using namespace eyelib::rendering;
        SceneDrawInstance visible{};
        visible.meshHandle = 0u;
        visible.materialId = imported.maxMaterialId;
        visible.transformIndex = 0u;
        visible.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        visible.localBounds = {{-1000.0f, -1000.0f, -1000.0f}, {1000.0f, 1000.0f, 1000.0f}};
        visible.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(visible);
    }

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> speedDist(0.35f, 1.25f);
    std::uniform_real_distribution<float> retargetDist(1.0f, 2.5f);

    eyelib::math::Vec3 rotationAxis{0.3f, 1.0f, 0.2f};
    rotationAxis = glm::normalize(rotationAxis);
    float rotationSpeed = speedDist(rng);
    float rotationAngle = 0.0f;
    float retargetTimer = retargetDist(rng);

    eyelib::rendering::RenderWorld world(device.get());
    world.SetOutputSize(kWidth, kHeight);
    world.SetSceneView(&sceneView);
    world.TrySubscribeFeature(&shadowFeature);
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    std::vector<std::shared_ptr<eyelib::rendering::Texture2D>> loadedTextures;
    loadedTextures.reserve(256);
    std::unordered_map<std::string, uint32_t> fileHandleCache;
    std::unordered_map<size_t, uint32_t> memoryHandleCache;
    const auto loadTextureHandle = [&](const TextureSource& src, bool isSrgb) -> uint32_t {
        using namespace eyelib::rendering;
        if (!src.valid)
            return materials::kInvalidMaterialTextureHandle;

        if (!src.filePath.empty()) {
            const auto it = fileHandleCache.find(src.filePath);
            if (it != fileHandleCache.end())
                return it->second;
        } else if (!src.encodedBytes.empty()) {
            const size_t key = std::hash<std::string_view>{}(std::string_view(
                reinterpret_cast<const char*>(src.encodedBytes.data()), src.encodedBytes.size()));
            const auto it = memoryHandleCache.find(key);
            if (it != memoryHandleCache.end())
                return it->second;
        }

        std::shared_ptr<Texture2D> tex = std::make_shared<Texture2D>();
        bool ok = false;
        if (!src.filePath.empty())
            ok = tex->LoadFromFile(device.get(), src.filePath, isSrgb, false);
        else if (!src.encodedBytes.empty())
            ok = tex->LoadFromMemory(device.get(), src.encodedBytes.data(), src.encodedBytes.size(), isSrgb, false);
        if (!ok || !tex->GetTextureRHI())
            return materials::kInvalidMaterialTextureHandle;

        const uint64_t handle64 = device->GetOrAllocateSharedBindlessTextureHandle(tex->GetTextureRHI());
        if (handle64 == 0u || handle64 > 0xFFFFFFFFull)
            return materials::kInvalidMaterialTextureHandle;
        const uint32_t handle = static_cast<uint32_t>(handle64);
        loadedTextures.push_back(tex);
        if (!src.filePath.empty()) {
            fileHandleCache[src.filePath] = handle;
        } else if (!src.encodedBytes.empty()) {
            const size_t key = std::hash<std::string_view>{}(std::string_view(
                reinterpret_cast<const char*>(src.encodedBytes.data()), src.encodedBytes.size()));
            memoryHandleCache[key] = handle;
        }
        return handle;
    };

    for (uint32_t materialId = 0; materialId < imported.materials.size(); ++materialId) {
        auto materialData = imported.materials[materialId];
        materialData.record.baseColorTexture = loadTextureHandle(materialData.baseColor, true);
        materialData.record.normalTexture = loadTextureHandle(materialData.normal, false);
        materialData.record.metallicRoughnessTexture = loadTextureHandle(materialData.metallicRoughness, false);
        materialData.record.emissiveTexture = loadTextureHandle(materialData.emissive, true);
        materialData.record.occlusionTexture = loadTextureHandle(materialData.occlusion, false);
        world.Resources().MaterialTable().SetMaterial(materialId, materialData.record);
    }

    double lastT = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currentT = glfwGetTime();
        float dt = static_cast<float>(currentT - lastT);
        lastT = currentT;

        static float camX = 0.0f, camY = 1.5f, camZ = 8.0f;
        static float camYaw = 0.0f, camPitch = 0.0f;
        static double lastMouseX = 0.0, lastMouseY = 0.0;
        static bool firstMouse = true;
        
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (firstMouse) { lastMouseX = mouseX; lastMouseY = mouseY; firstMouse = false; }
            float xoffset = static_cast<float>(mouseX - lastMouseX);
            float yoffset = static_cast<float>(lastMouseY - mouseY);
            lastMouseX = mouseX; lastMouseY = mouseY;
            
            float sensitivity = 0.1f;
            camYaw += xoffset * sensitivity; // Swapped drag direction: moving mouse right rotates camera right.
            camPitch += yoffset * sensitivity;
            if (camPitch > 89.0f) camPitch = 89.0f;
            if (camPitch < -89.0f) camPitch = -89.0f;
        } else {
            firstMouse = true;
        }
        
        const float yawRad = glm::radians(camYaw);
        const float pitchRad = glm::radians(camPitch);
        eyelib::math::Vec3 front = eyelib::rendering::camera::ComputeForward(yawRad, pitchRad);
        
        eyelib::math::Vec3 right = glm::normalize(glm::cross(front, eyelib::math::Vec3(0.0f, 1.0f, 0.0f)));
        
        float velocity = 50.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) velocity *= 3.0f;
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camX += front.x * velocity; camY += front.y * velocity; camZ += front.z * velocity; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camX -= front.x * velocity; camY -= front.y * velocity; camZ -= front.z * velocity; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camX -= right.x * velocity; camY -= right.y * velocity; camZ -= right.z * velocity; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camX += right.x * velocity; camY += right.y * velocity; camZ += right.z * velocity; }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { camY -= velocity; }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { camY += velocity; }

        int frameW = 0;
        int frameH = 0;
        glfwGetFramebufferSize(window, &frameW, &frameH);
        if (frameW > 0 && frameH > 0) {
            world.SetOutputSize(static_cast<uint32_t>(frameW), static_cast<uint32_t>(frameH));
            device->OnWindowResized(static_cast<uint32_t>(frameW), static_cast<uint32_t>(frameH));

            eyelib::rendering::camera::FreeFlyCameraDesc camDesc{};
            camDesc.position = {camX, camY, camZ};
            camDesc.yaw = yawRad;
            camDesc.pitch = pitchRad;
            camDesc.fovDegrees = 60.0f;
            camDesc.nearPlane = 0.1f;
            camDesc.farPlane = 1000.0f;
            sceneView.camera = eyelib::rendering::camera::BuildFreeFlyCameraView(
                camDesc, static_cast<float>(frameW), static_cast<float>(frameH));
            sceneView.camera = eyelib::rendering::camera::AdjustCameraForBackend(
                sceneView.camera, eyelib::rendering::RenderBackend::Vulkan);
        }



        retargetTimer -= dt;
        if (retargetTimer <= 0.0f) {
            eyelib::math::Vec3 candidate{axisDist(rng), axisDist(rng), axisDist(rng)};
            if (glm::dot(candidate, candidate) < 1e-4f)
                candidate = {0.0f, 1.0f, 0.0f};
            rotationAxis = glm::normalize(candidate);
            rotationSpeed = speedDist(rng);
            retargetTimer = retargetDist(rng);
        }
        rotationAngle += dt * rotationSpeed;
        // Keep static transform for asset inspection; leave camera as the primary interaction.
        sceneView.drawInstances[0].worldTransform = glm::scale(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.2f));

        device->BeginFrame();
        eyelib::rendering::rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (cmd)
            cmd->Begin();
        world.RunFrame(dt, cmd);
        device->EndFrame();
        device->Present();
    }

    world.Shutdown();
    sandboxMesh->Destroy(device.get());
    sandboxMesh.reset();
    device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
