#include "api/core/All.h"
#include "api/math/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "api/physics/All.h"
#include "api/animation/All.h"
#include "api/ecs/All.h"
#include "api/scene/All.h"
#include "api/assets/All.h"
#include "api/serialize/All.h"
#include "api/particles/All.h"
#include "api/fluids/All.h"
#include "api/audio/All.h"
#include "api/debug/All.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <glm/gtc/matrix_transform.hpp>

namespace {

struct TestStats {
    int passed = 0;
    int failed = 0;
};

bool Expect(TestStats& stats, bool condition, const std::string& name) {
    if (condition) {
        stats.passed++;
        std::cout << "[PASS] " << name << "\n";
        return true;
    }
    stats.failed++;
    std::cout << "[FAIL] " << name << "\n";
    eyelib::api::core::Log::WarnF("Test failed: %s", name.c_str());
    return false;
}

std::shared_ptr<eyelib::api::render::Mesh> BuildPlaneMesh(float halfSize) {
    using namespace eyelib::api::render;
    auto mesh = std::make_shared<Mesh>();

    std::vector<Mesh::Vertex> vertices(4);
    vertices[0].position = {-halfSize, 0.0f, -halfSize};
    vertices[1].position = { halfSize, 0.0f, -halfSize};
    vertices[2].position = { halfSize, 0.0f,  halfSize};
    vertices[3].position = {-halfSize, 0.0f,  halfSize};
    for (auto& v : vertices) {
        v.normal = {0.0f, 1.0f, 0.0f};
    }
    vertices[0].uv = {0.0f, 0.0f};
    vertices[1].uv = {1.0f, 0.0f};
    vertices[2].uv = {1.0f, 1.0f};
    vertices[3].uv = {0.0f, 1.0f};

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    return mesh;
}

std::shared_ptr<eyelib::api::render::Mesh> BuildCubeMesh(float halfSize) {
    using namespace eyelib::api::render;
    auto mesh = std::make_shared<Mesh>();

    struct Face {
        eyelib::math::Vec3 normal;
        eyelib::math::Vec3 v0, v1, v2, v3;
    };
    const float h = halfSize;
    const Face faces[] = {
        {{ 0, 0, 1}, {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}}, // front
        {{ 0, 0,-1}, { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}}, // back
        {{ 1, 0, 0}, { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h}}, // right
        {{-1, 0, 0}, {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h}}, // left
        {{ 0, 1, 0}, {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h}}, // top
        {{ 0,-1, 0}, {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}}  // bottom
    };

    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    const float uvInset = 0.02f;
    const float u0 = uvInset;
    const float v0uv = uvInset;
    const float u1 = 1.0f - uvInset;
    const float v1uv = 1.0f - uvInset;

    for (const auto& face : faces) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        Mesh::Vertex v0{}, v1{}, v2{}, v3{};
        v0.position = face.v0; v1.position = face.v1; v2.position = face.v2; v3.position = face.v3;
        v0.normal = v1.normal = v2.normal = v3.normal = face.normal;
        v0.uv = {u0, v0uv};
        v1.uv = {u1, v0uv};
        v2.uv = {u1, v1uv};
        v3.uv = {u0, v1uv};
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    return mesh;
}

bool TestRender(TestStats& stats) {
    using namespace eyelib::api::platform;
    using namespace eyelib::api::render;
    using namespace eyelib::api::animation;
    using namespace eyelib::api::assets;
    using namespace eyelib::math;

    Window window;
    WindowConfig cfg;
    cfg.title = "OpenEye API Tests";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.debugContext = false;
    cfg.resizable = true;

    if (!Expect(stats, window.Initialize(cfg), "Render: window init")) {
        return false;
    }

    Renderer renderer;
    if (!Expect(stats, renderer.Initialize(), "Render: renderer init")) {
        window.Shutdown();
        return false;
    }

    TextureCube skybox;
    std::vector<std::string> skyFaces = {
        "assets/Clear Ocean/vz_clear_ocean_right.png",
        "assets/Clear Ocean/vz_clear_ocean_left.png",
        "assets/Clear Ocean/vz_clear_ocean_up.png",
        "assets/Clear Ocean/vz_clear_ocean_down.png",
        "assets/Clear Ocean/vz_clear_ocean_front.png",
        "assets/Clear Ocean/vz_clear_ocean_back.png"
    };
    bool skyLoaded = skybox.LoadFromFiles(skyFaces, true, true);
    Expect(stats, skyLoaded, "Render: load Clear Ocean skybox");
    skybox.SetFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
    skybox.SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);

    const std::string modelPath = "assets/models/dess_holiday_hb_juan_deltarune/scene.gltf";
    auto modelAsset = ModelLoader::LoadModel(modelPath);
    Expect(stats, modelAsset && !modelAsset->GetMeshes().empty(), "Render: load dess_holiday model");
    std::vector<std::shared_ptr<Mesh>> modelMeshes;
    std::vector<std::shared_ptr<Material>> modelMaterials;
    if (modelAsset) {
        modelMeshes = modelAsset->GetMeshes();
        modelMaterials = modelAsset->GetMaterials();
    }

    const std::string animModelPath = "assets/models/animation/Orc Idle.fbx";
    auto animAsset = ModelLoader::LoadModel(animModelPath);
    Expect(stats, animAsset && !animAsset->GetMeshes().empty(), "Render: load animation model");
    std::vector<std::shared_ptr<Mesh>> animMeshes;
    std::vector<std::shared_ptr<Material>> animMaterials;
    if (animAsset) {
        animMeshes = animAsset->GetMeshes();
        animMaterials = animAsset->GetMaterials();
    }

    const float animScale = 0.12f;
    const Vec3 animOffset(-2.5f, 0.0f, 0.0f);
    const Mat4 animModelTransform = glm::translate(Mat4(1.0f), animOffset) *
                                    glm::scale(Mat4(1.0f), Vec3(animScale));

    auto animSkeleton = animAsset ? animAsset->GetSkeleton() : nullptr;
    auto animClip = animAsset ? animAsset->GetClip() : nullptr;
    if (animClip) animClip->SetLoop(true);
    Expect(stats, animSkeleton && animClip, "Render: load animation clip");
    auto animator = std::make_shared<Animator>();
    if (animSkeleton) animator->SetSkeleton(animSkeleton);
    if (animClip) animator->SetClip(animClip);

    RenderTarget colorOnly = RenderTarget::CreateColorOnly(128, 128);
    Expect(stats, colorOnly.GetColorAttachmentCount() == 1, "Render: color-only target");
    Expect(stats, colorOnly.GetDepthTexture() == nullptr, "Render: no depth texture");

    RenderPipeline pipeline;
    if (!Expect(stats, pipeline.Initialize(window.Width(), window.Height()), "Render: pipeline init")) {
        renderer.Shutdown();
        window.Shutdown();
        return false;
    }
    pipeline.SetMode(RenderPipeline::Mode::Forward);

    RenderScene scene;
    scene.skybox = std::make_shared<TextureCube>(std::move(skybox));
    Vec3 cameraPos(2.5f, 2.0f, 2.5f);
    float cameraYaw = -135.0f;
    float cameraPitch = -20.0f;
    scene.camera.projection = glm::perspective(glm::radians(60.0f), window.AspectRatio(), 0.1f, 1000.0f);
    scene.camera.viewPos = cameraPos;

    scene.light.direction = {-0.4f, -1.0f, -0.2f};
    scene.light.color = {1.0f, 1.0f, 1.0f};
    scene.light.intensity = 2.0f;
    scene.light.ambient = Vec3(0.1f);
    scene.light.exposure = 1.0f;

    for (const auto& mat : modelMaterials) {
        if (!mat) continue;
        mat->SetVec3("u_LightDir", Vec3(-0.4f, -1.0f, -0.2f));
        mat->SetVec3("u_LightColor", Vec3(1.0f, 1.0f, 1.0f));
        mat->SetVec3("u_Emission", Vec3(0.0f));
    }
    for (const auto& mat : animMaterials) {
        if (!mat) continue;
        mat->SetVec3("u_LightDir", Vec3(-0.4f, -1.0f, -0.2f));
        mat->SetVec3("u_LightColor", Vec3(1.0f, 1.0f, 1.0f));
        mat->SetVec3("u_Emission", Vec3(0.0f));
    }

    AABB bounds;
    bool boundsInitialized = false;
    for (const auto& mesh : modelMeshes) {
        if (!mesh || !mesh->IsValid()) continue;
        if (!boundsInitialized) {
            bounds = mesh->GetAABB();
            boundsInitialized = true;
        } else {
            bounds = bounds.Union(mesh->GetAABB());
        }
        Drawable drawable{};
        drawable.mesh = mesh;
        if (!modelMaterials.empty() && mesh->GetSubmeshCount() > 0) {
            const int matIndex = mesh->GetSubmesh(0).materialIndex;
            if (matIndex >= 0 && static_cast<size_t>(matIndex) < modelMaterials.size()) {
                drawable.material = modelMaterials[static_cast<size_t>(matIndex)];
            }
        }
        if (!drawable.material && !modelMaterials.empty()) {
            drawable.material = modelMaterials.front();
        }
        drawable.transform = glm::mat4(1.0f);
        scene.opaque.push_back(drawable);
    }
    Expect(stats, !scene.opaque.empty(), "Render: pipeline render dess_holiday");

    for (const auto& mesh : animMeshes) {
        if (!mesh || !mesh->IsValid()) continue;
        Drawable drawable{};
        drawable.mesh = mesh;
        if (!animMaterials.empty() && mesh->GetSubmeshCount() > 0) {
            const int matIndex = mesh->GetSubmesh(0).materialIndex;
            if (matIndex >= 0 && static_cast<size_t>(matIndex) < animMaterials.size()) {
                drawable.material = animMaterials[static_cast<size_t>(matIndex)];
            }
        }
        if (!drawable.material && !animMaterials.empty()) {
            drawable.material = animMaterials.front();
        }
        drawable.transform = animModelTransform;
        scene.opaque.push_back(drawable);
    }
    Expect(stats, !animMeshes.empty(), "Render: pipeline render animation model");

    if (boundsInitialized) {
        Vec3 center = bounds.GetCenter();
        Vec3 size = bounds.GetSize();
        float radius = glm::length(size) * 0.6f;
        cameraPos = center + Vec3(0.0f, radius * 0.4f, radius * 2.2f);
        scene.camera.viewPos = cameraPos;
    }

    const auto startTime = std::chrono::steady_clock::now();
    auto lastTime = startTime;
    int lastWidth = window.Width();
    int lastHeight = window.Height();

    const int KEY_W = 87;
    const int KEY_A = 65;
    const int KEY_S = 83;
    const int KEY_D = 68;
    const int KEY_Q = 81;
    const int KEY_E = 69;
    const int MOUSE_RIGHT = 1;

    while (!window.ShouldClose()) {
        window.PollEvents();
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - startTime;

        const int w = window.Width();
        const int h = window.Height();
        if (w != lastWidth || h != lastHeight) {
            lastWidth = w;
            lastHeight = h;
            pipeline.Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            scene.camera.projection = glm::perspective(glm::radians(60.0f), window.AspectRatio(), 0.1f, 1000.0f);
        }

        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        lastTime = now;
        Vec3 forward;
        forward.x = std::cos(glm::radians(cameraPitch)) * std::cos(glm::radians(cameraYaw));
        forward.y = std::sin(glm::radians(cameraPitch));
        forward.z = std::cos(glm::radians(cameraPitch)) * std::sin(glm::radians(cameraYaw));
        forward = glm::normalize(forward);
        const Vec3 up(0.0f, 1.0f, 0.0f);
        const Vec3 right = glm::normalize(glm::cross(forward, up));

        const float speed = 3.5f;
        const float move = speed * deltaTime;
        if (Input::IsKeyPressed(KEY_W)) cameraPos += forward * move;
        if (Input::IsKeyPressed(KEY_S)) cameraPos -= forward * move;
        if (Input::IsKeyPressed(KEY_D)) cameraPos += right * move;
        if (Input::IsKeyPressed(KEY_A)) cameraPos -= right * move;
        if (Input::IsKeyPressed(KEY_E)) cameraPos += up * move;
        if (Input::IsKeyPressed(KEY_Q)) cameraPos -= up * move;

        if (Input::IsMouseButtonPressed(MOUSE_RIGHT)) {
            const auto mouseDelta = Input::MouseDelta();
            const float sensitivity = 0.15f;
            cameraYaw += mouseDelta.x * sensitivity;
            cameraPitch -= mouseDelta.y * sensitivity;
            if (cameraPitch > 89.0f) cameraPitch = 89.0f;
            if (cameraPitch < -89.0f) cameraPitch = -89.0f;
        }

        scene.camera.view = glm::lookAt(cameraPos, cameraPos + forward, up);
        scene.camera.viewPos = cameraPos;

        const float t = elapsed.count();
        if (!scene.opaque.empty()) {
            scene.opaque[0].transform = glm::rotate(glm::mat4(1.0f), t * 0.3f, Vec3(0.0f, 1.0f, 0.0f));
        }

        if (animator && animSkeleton && animClip) {
            animator->Update(deltaTime);
            const auto& bones = animator->GetBoneMatrices();
            for (const auto& mat : animMaterials) {
                if (!mat) continue;
                mat->SetMat4Array("u_Bones", bones);
            }
        }

        renderer.Clear(0.1f, 0.1f, 0.1f, 1.0f);
        pipeline.Render(scene);
        pipeline.BlitToScreen();
        window.SwapBuffers();

        const auto frameEnd = std::chrono::steady_clock::now();
        const auto frameMs = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - lastTime);
        if (frameMs.count() < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameMs.count()));
        }
        Input::Update();
    }

    pipeline.Shutdown();
    renderer.Shutdown();
    window.Shutdown();
    return true;
}

} // namespace

int main() {
    using eyelib::api::core::Log;
    using eyelib::api::core::LogLevel;

    Log::Initialize("OpenEyeApiTests.log");
    Log::SetLevel(LogLevel::Info);

    TestStats stats{};
    TestRender(stats);

    Log::InfoF("Tests finished. Passed: %d, Failed: %d", stats.passed, stats.failed);
    Log::Shutdown();

    return stats.failed == 0 ? 0 : 1;
}
