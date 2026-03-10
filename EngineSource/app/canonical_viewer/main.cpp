/**
 * Canonical viewer: loads .mesh (single) or .prefab (whole model with materials + textures).
 * Camera: same as editor — right-drag look, WASD + Q/E move. Forward + passes/forward/lit.
 * Usage: CanonicalViewer <path_to.mesh> | CanonicalViewer <path_to.prefab>
 */

#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "asset_pipe/readers/mesh_reader.h"
#include "asset_pipe/readers/prefab_reader.h"
#include "asset_pipe/readers/material_reader.h"
#include "asset_pipe/readers/texture_reader.h"
#include "asset_pipe/core/import_mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/rhi/RHI.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

namespace fs = std::filesystem;

std::string Dirname(const std::string& path) {
    fs::path p(path);
    return p.parent_path().string();
}

std::shared_ptr<eyelib::api::Mesh> LoadCanonicalMesh(const std::string& path, eyelib::api::RenderDevice device) {
    asset_pipe::ImportMesh importMesh;
    if (!asset_pipe::MeshReader::Read(path, importMesh)) {
        std::cerr << "CanonicalViewer: failed to read " << path << "\n";
        return nullptr;
    }

    auto mesh = std::make_shared<eyelib::api::Mesh>();
    std::vector<eyelib::api::Mesh::Vertex> vertices;
    vertices.reserve(importMesh.vertices.size());
    for (const auto& v : importMesh.vertices) {
        eyelib::api::Mesh::Vertex vert;
        vert.position = eyelib::math::Vec3(v.position.x, v.position.y, v.position.z);
        vert.normal = eyelib::math::Vec3(v.normal.x, v.normal.y, v.normal.z);
        vert.uv = eyelib::math::Vec2(v.uv0.x, v.uv0.y);
        vert.uv1 = eyelib::math::Vec2(v.uv1.x, v.uv1.y);
        vert.tangent = eyelib::math::Vec3(v.tangent.x, v.tangent.y, v.tangent.z);
        vert.color = eyelib::math::Vec4(1.f, 1.f, 1.f, 1.f);
        vert.boneIndices = eyelib::math::IVec4(0, 0, 0, 0);
        vert.boneWeights = eyelib::math::Vec4(0.f, 0.f, 0.f, 0.f);
        vertices.push_back(vert);
    }
    mesh->SetVertices(vertices);
    mesh->SetIndices(importMesh.indices);
    mesh->ClearSubmeshes();
    for (const auto& sub : importMesh.submeshes) {
        mesh->AddSubmesh(sub.indexStart, sub.indexCount, static_cast<int>(sub.materialIndex));
    }
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<eyelib::api::Texture> LoadTextureFromPath(eyelib::api::RenderDevice device, const std::string& path) {
    if (path.empty()) return nullptr;
    auto apiTex = std::make_shared<eyelib::api::Texture>();
    std::shared_ptr<eyelib::render::Texture2D> internal = apiTex->GetInternal();
    if (!internal || !device) return nullptr;
    auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(device.Get());
    internal->SetDevice(rhiDevice);

    const bool isTex = (path.size() >= 4 && path.compare(path.size() - 4, 4, ".tex") == 0);
    if (isTex) {
        asset_pipe::LoadedTexture loaded;
        if (!asset_pipe::TextureReader::Read(path, loaded) || loaded.pixels.empty()) return nullptr;
        eyelib::api::TextureFormat fmt = (loaded.channels == 4) ? eyelib::api::TextureFormat::RGBA8 : eyelib::api::TextureFormat::RGB8;
        if (!apiTex->Create(static_cast<int>(loaded.width), static_cast<int>(loaded.height), fmt, loaded.pixels.data())) return nullptr;
        apiTex->GenerateMipmaps();
        return apiTex;
    }
    if (!apiTex->LoadFromFile(path, true, true)) return nullptr;
    return apiTex;
}

std::shared_ptr<eyelib::api::Material> CreateMaterialFromLoaded(eyelib::api::RenderDevice device,
    const asset_pipe::LoadedMaterial& loaded,
    const std::string& baseDir,
    std::shared_ptr<eyelib::api::Shader> litShader) {
    if (!litShader) return nullptr;
    auto mat = std::make_shared<eyelib::api::Material>();
    mat->SetShader(litShader);
    mat->SetAlbedo(eyelib::math::Vec3(0.85f, 0.85f, 0.88f));
    mat->SetMetallic(loaded.metallicValue);
    mat->SetRoughness(loaded.roughnessValue);
    auto resolvePath = [&baseDir](const std::string& p) -> std::string {
        if (p.empty()) return {};
        fs::path path(p);
        if (path.is_absolute()) return p;
        return (fs::path(baseDir) / p).string();
    };
    if (!loaded.albedoPath.empty()) {
        std::string p = resolvePath(loaded.albedoPath);
        if (!p.empty()) {
            if (auto tex = LoadTextureFromPath(device, p)) mat->SetAlbedoTexture(tex);
        }
    }
    if (!loaded.normalPath.empty()) {
        std::string p = resolvePath(loaded.normalPath);
        if (!p.empty()) {
            if (auto tex = LoadTextureFromPath(device, p)) mat->SetNormalTexture(tex);
        }
    }
    if (!loaded.metallicPath.empty()) {
        std::string p = resolvePath(loaded.metallicPath);
        if (!p.empty()) {
            if (auto tex = LoadTextureFromPath(device, p)) mat->SetMetallicTexture(tex);
        }
    }
    if (!loaded.roughnessPath.empty()) {
        std::string p = resolvePath(loaded.roughnessPath);
        if (!p.empty()) {
            if (auto tex = LoadTextureFromPath(device, p)) mat->SetRoughnessTexture(tex);
        }
    }
    return mat;
}

std::shared_ptr<eyelib::api::Material> CreateDefaultMaterial(eyelib::api::RenderDevice device) {
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (!device || !shader->Load(device, "passes/forward/lit")) {
        eyelib::api::core::Log::Warn("CanonicalViewer: default shader (passes/forward/lit) not loaded.");
        return nullptr;
    }
    auto mat = std::make_shared<eyelib::api::Material>();
    mat->SetShader(shader);
    mat->SetAlbedo(eyelib::math::Vec3(0.85f, 0.85f, 0.88f));
    mat->SetMetallic(0.f);
    mat->SetRoughness(0.5f);
    return mat;
}

// Row-major 16 floats (prefab) -> column-major glm::mat4
glm::mat4 ToGlmMat4(const float* m) {
    return glm::mat4(
        m[0], m[4], m[8], m[12],
        m[1], m[5], m[9], m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15]
    );
}

// Match editor EditorCamera + SceneViewPanel exactly: right-drag look, WASD + Q/E move
constexpr int kMouseButtonRight = 1;
constexpr float kPitchMin = -1.55f;
constexpr float kPitchMax = 1.55f;
// GLFW key codes (letters)
constexpr int kKeyW = 87, kKeyA = 65, kKeyS = 83, kKeyD = 68, kKeyQ = 81, kKeyE = 69;

struct FreeFlyCamera {
    glm::vec3 position{0.f, 0.f, 10.f};
    float yaw = 0.f;      // radians (around world Y)
    float pitch = 0.f;    // radians (look up/down)
    float fovDegrees = 60.f;
    float moveSpeed = 8.f;
    float mouseSensitivity = 0.002f;
};

// Same as editor BuildViewProjection: forward = (cy*st, sy, -cy*ct), target = position + forward
static void ApplyFreeFlyCamera(eyelib::api::RenderScene& scene, const FreeFlyCamera& cam) {
    float cy = std::cos(cam.pitch), sy = std::sin(cam.pitch);
    float ct = std::cos(cam.yaw), st = std::sin(cam.yaw);
    glm::vec3 forward(cy * st, sy, -cy * ct);
    glm::vec3 target = cam.position + forward;
    scene.camera.view = glm::lookAt(cam.position, target, glm::vec3(0.f, 1.f, 0.f));
    scene.camera.viewPos = eyelib::math::Vec3(cam.position.x, cam.position.y, cam.position.z);
}

void SetupCameraAndLight(eyelib::api::RenderScene& scene, const eyelib::math::AABB& sceneAABB, float radius, FreeFlyCamera& cam) {
    eyelib::math::Vec3 center = sceneAABB.IsValid() ? sceneAABB.GetCenter() : eyelib::math::Vec3(0.f, 0.f, 0.f);
    if (sceneAABB.IsValid()) {
        eyelib::math::Vec3 size = sceneAABB.GetSize();
        radius = glm::length(glm::vec3(size.x, size.y, size.z)) * 0.5f;
    }
    if (radius < 0.1f) radius = 5.f;
    cam.position = glm::vec3(center.x, center.y, center.z) + glm::vec3(0.f, 0.f, radius * 1.5f);
    cam.yaw = 0.f;
    cam.pitch = 0.2f;
    scene.camera.projection = glm::perspective(glm::radians(cam.fovDegrees), static_cast<float>(kWidth) / static_cast<float>(kHeight), 0.1f, 1000.f);
    scene.camera.nearPlane = 0.1f;
    scene.camera.farPlane = 1000.f;
    scene.light.direction = glm::normalize(eyelib::math::Vec3(0.5f, 1.f, 0.3f));
    scene.light.color = eyelib::math::Vec3(1.f, 0.98f, 0.95f);
    scene.light.intensity = 2.f;
    scene.light.ambient = eyelib::math::Vec3(0.35f);  // higher ambient so model is visible clearly in viewer
    scene.light.exposure = 1.f;
    scene.light.castShadows = false;
    scene.skybox.reset();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: CanonicalViewer <path_to.mesh | path_to.prefab>\n";
        std::cerr << "  .mesh = single mesh with default material. .prefab = full model with materials.\n";
        return 1;
    }

    const std::string path = argv[1];
    const bool isPrefab = (path.size() >= 7 && path.compare(path.size() - 7, 7, ".prefab") == 0);

    eyelib::api::core::Engine::Config engineConfig{};
    engineConfig.logFile = "CanonicalViewer.log";
    if (!eyelib::api::core::Engine::Initialize(engineConfig)) {
        std::cerr << "CanonicalViewer: Engine init failed\n";
        return 2;
    }

    eyelib::api::Window window;
    eyelib::api::WindowConfig winConfig{};
    winConfig.title = "Canonical Viewer";
    winConfig.width = kWidth;
    winConfig.height = kHeight;
    if (!window.Initialize(winConfig)) {
        std::cerr << "CanonicalViewer: Window init failed\n";
        eyelib::api::core::Engine::Shutdown();
        return 3;
    }

    eyelib::api::RenderPipeline pipeline;
    if (!pipeline.Initialize(window, kWidth, kHeight, eyelib::render::RenderBackend::Default)) {
        std::cerr << "CanonicalViewer: Pipeline init failed\n";
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 4;
    }
    pipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);

    eyelib::api::RenderDevice device = pipeline.GetDevice();
    if (!device) {
        std::cerr << "CanonicalViewer: No render device\n";
        pipeline.Shutdown();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 5;
    }

    auto litShader = std::make_shared<eyelib::api::Shader>();
    if (!litShader->Load(device, "passes/forward/lit")) {
        std::cerr << "CanonicalViewer: passes/forward/lit shader not found. Run from Engine/bin/Debug or set cwd.\n";
        pipeline.Shutdown();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 6;
    }

    eyelib::api::RenderScene scene{};
    scene.transparent = nullptr;
    scene.transparentCount = 0;
    scene.pointLights = nullptr;
    scene.pointLightCount = 0;
    scene.spotLights = nullptr;
    scene.spotLightCount = 0;

    eyelib::math::AABB sceneAABB;
    FreeFlyCamera freeFlyCam;

    if (isPrefab) {
        std::vector<asset_pipe::PrefabDrawable> drawables;
        if (!asset_pipe::PrefabReader::Read(path, drawables)) {
            std::cerr << "CanonicalViewer: failed to read prefab " << path << "\n";
            pipeline.Shutdown();
            window.Shutdown();
            eyelib::api::core::Engine::Shutdown();
            return 7;
        }

        std::string baseDir = Dirname(path);
        if (baseDir.empty()) baseDir = ".";

        std::vector<asset_pipe::LoadedMaterial> loadedMats;
        std::vector<std::string> matFiles;
        try {
            for (const auto& e : fs::directory_iterator(baseDir)) {
                if (e.path().extension() == ".material")
                    matFiles.push_back(e.path().string());
            }
        } catch (...) {}
        // Sort by numeric suffix (e.g. house_material_0, house_mat1) so order matches importer material indices
        std::sort(matFiles.begin(), matFiles.end(), [](const std::string& a, const std::string& b) {
            auto num = [](const std::string& s) {
                int n = -1;
                size_t i = s.find_last_of("0123456789");
                if (i != std::string::npos) {
                    size_t start = s.find_last_not_of("0123456789", i);
                    start = (start == std::string::npos) ? 0 : start + 1;
                    try { n = std::stoi(s.substr(start)); } catch (...) {}
                }
                return n;
            };
            int na = num(a), nb = num(b);
            if (na != nb) return na < nb;
            return a < b;
        });
        for (const auto& p : matFiles) {
            asset_pipe::LoadedMaterial lm;
            if (asset_pipe::MaterialReader::Read(p, lm))
                loadedMats.push_back(std::move(lm));
        }

        std::vector<std::shared_ptr<eyelib::api::Material>> materials;
        materials.push_back(CreateDefaultMaterial(device));
        for (size_t i = 0; i < loadedMats.size(); ++i) {
            auto m = CreateMaterialFromLoaded(device, loadedMats[i], baseDir, litShader);
            materials.push_back(m ? m : materials[0]);
        }

        for (const auto& d : drawables) {
            std::string meshPath = (fs::path(baseDir) / d.meshPath).string();
            auto mesh = LoadCanonicalMesh(meshPath, device);
            if (!mesh || !mesh->IsValid()) continue;

            eyelib::api::Drawable draw;
            draw.mesh = mesh;
            draw.materialInstance.reset();
            int matIndex = 0;
            if (mesh->GetSubmeshCount() > 0)
                matIndex = mesh->GetSubmesh(0).materialIndex;
            size_t matSlot = (matIndex >= 0 && static_cast<size_t>(matIndex) < loadedMats.size())
                ? static_cast<size_t>(matIndex) + 1u  // +1: slot 0 is default
                : 0u;
            if (matSlot >= materials.size()) matSlot = 0;
            draw.material = materials[matSlot];
            if (draw.material)
                eyelib::api::FillDrawableRenderParams(draw.params, draw.material, draw.materialInstance);
            draw.transform = ToGlmMat4(&d.worldTransform.m[0]);
            draw.bones = nullptr;
            draw.boneCount = 0;
            scene.opaqueStorage.push_back(std::move(draw));

            if (mesh->GetAABB().IsValid()) {
                const eyelib::math::AABB worldAABB = mesh->GetAABB().Transform(draw.transform);
                sceneAABB = sceneAABB.IsValid() ? sceneAABB.Union(worldAABB) : worldAABB;
            }
        }
        scene.opaqueCount = scene.opaqueStorage.size();
        scene.opaque = scene.opaqueStorage.data();
        float radius = 5.f;
        SetupCameraAndLight(scene, sceneAABB, radius, freeFlyCam);
        std::cout << "CanonicalViewer: prefab " << path << " (" << scene.opaqueCount << " meshes, " << materials.size() << " materials). Right-drag look, WASD+Q/E move.\n";
    } else {
        auto defaultMaterial = CreateDefaultMaterial(device);
        auto mesh = LoadCanonicalMesh(path, device);
        if (!mesh || !mesh->IsValid()) {
            std::cerr << "CanonicalViewer: Failed to load mesh " << path << "\n";
            pipeline.Shutdown();
            window.Shutdown();
            eyelib::api::core::Engine::Shutdown();
            return 7;
        }

        scene.opaqueStorage.resize(1);
        scene.opaqueStorage[0].mesh = mesh;
        scene.opaqueStorage[0].material = defaultMaterial;
        scene.opaqueStorage[0].materialInstance.reset();
        if (defaultMaterial)
            eyelib::api::FillDrawableRenderParams(scene.opaqueStorage[0].params, defaultMaterial, nullptr);
        scene.opaqueStorage[0].transform = glm::mat4(1.f);
        scene.opaqueStorage[0].bones = nullptr;
        scene.opaqueStorage[0].boneCount = 0;
        scene.opaque = scene.opaqueStorage.data();
        scene.opaqueCount = 1;
        sceneAABB = mesh->GetAABB();
        SetupCameraAndLight(scene, sceneAABB, 5.f, freeFlyCam);
        std::cout << "CanonicalViewer: mesh " << path << ". Right-drag look, WASD+Q/E move.\n";
    }

    auto lastTime = std::chrono::steady_clock::now();
    while (!window.ShouldClose()) {
        window.PollEvents();

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (dt <= 0.f || dt > 0.5f) dt = 0.016f;

        // Right-drag: rotate (same as editor SceneViewPanel)
        if (eyelib::api::Input::IsMouseButtonPressed(kMouseButtonRight)) {
            auto delta = eyelib::api::Input::MouseDelta();
            freeFlyCam.yaw += delta.x * freeFlyCam.mouseSensitivity;
            freeFlyCam.pitch -= delta.y * freeFlyCam.mouseSensitivity;
            if (freeFlyCam.pitch > kPitchMax) freeFlyCam.pitch = kPitchMax;
            if (freeFlyCam.pitch < kPitchMin) freeFlyCam.pitch = kPitchMin;
        }

        // WASD + Q/E: free-fly move (same as editor)
        float cy = std::cos(freeFlyCam.pitch), sy = std::sin(freeFlyCam.pitch);
        float ct = std::cos(freeFlyCam.yaw), st = std::sin(freeFlyCam.yaw);
        float forwardX = cy * st, forwardY = sy, forwardZ = -cy * ct;
        float rx = cy * ct, rz = cy * st;
        float rlen = std::sqrt(rx * rx + rz * rz);
        if (rlen > 1e-5f) { rx /= rlen; rz /= rlen; }
        float move = freeFlyCam.moveSpeed * dt;
        if (eyelib::api::Input::IsKeyPressed(kKeyW)) { freeFlyCam.position.x += forwardX * move; freeFlyCam.position.y += forwardY * move; freeFlyCam.position.z += forwardZ * move; }
        if (eyelib::api::Input::IsKeyPressed(kKeyS)) { freeFlyCam.position.x -= forwardX * move; freeFlyCam.position.y -= forwardY * move; freeFlyCam.position.z -= forwardZ * move; }
        if (eyelib::api::Input::IsKeyPressed(kKeyA)) { freeFlyCam.position.x -= rx * move; freeFlyCam.position.z -= rz * move; }
        if (eyelib::api::Input::IsKeyPressed(kKeyD)) { freeFlyCam.position.x += rx * move; freeFlyCam.position.z += rz * move; }
        if (eyelib::api::Input::IsKeyPressed(kKeyQ)) { freeFlyCam.position.y -= move; }
        if (eyelib::api::Input::IsKeyPressed(kKeyE)) { freeFlyCam.position.y += move; }

        ApplyFreeFlyCamera(scene, freeFlyCam);

        pipeline.Render(scene);
        pipeline.BlitToScreen();
        window.SwapBuffers();
    }

    pipeline.Shutdown();
    window.Shutdown();
    eyelib::api::core::Engine::Shutdown();
    return 0;
}
