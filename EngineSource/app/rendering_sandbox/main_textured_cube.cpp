// Textured rotating cube: EyelibRendering + GLFW + Vulkan (deferred + bindless checker texture).
// Separate entry from main.cpp (Sponza sandbox).
#include "rendering/RenderingModule.h"

#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/RenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/materials/MaterialSchema.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

const char* ModeName(eyelib::rendering::RenderMode mode) {
    return mode == eyelib::rendering::RenderMode::Deferred ? "Deferred" : "Forward";
}

eyelib::rendering::MeshDescriptor MakeTexturedCubeMeshDescriptor() {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    const float h = 1.0f;
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

    addFace({0.0f, 0.0f, 1.0f}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
    addFace({0.0f, 0.0f, -1.0f}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});
    addFace({1.0f, 0.0f, 0.0f}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});
    addFace({-1.0f, 0.0f, 0.0f}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});
    addFace({0.0f, 1.0f, 0.0f}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});
    addFace({0.0f, -1.0f, 0.0f}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});

    // Keep checker mesh as single draw without submesh material override so
    // SceneDrawInstance::materialId drives per-instance parity cases.
    return d;
}

std::string ResolveCheckerTexturePath() {
    namespace fs = std::filesystem;
    const std::array<const char*, 8> candidates = {
        "assets/textures/checker.png",
        "Engine/assets/textures/checker.png",
        "../assets/textures/checker.png",
        "../Engine/assets/textures/checker.png",
        "../../assets/textures/checker.png",
        "../../Engine/assets/textures/checker.png",
        "../../../assets/textures/checker.png",
        "../../../Engine/assets/textures/checker.png",
    };
    for (const char* rel : candidates) {
        const fs::path p(rel);
        if (fs::exists(p))
            return p.lexically_normal().string();
    }
    return {};
}

std::string ResolveAltCheckerTexturePath() {
    namespace fs = std::filesystem;
    const std::array<const char*, 8> candidates = {
        "assets/textures/checke.png",
        "Engine/assets/textures/checke.png",
        "../assets/textures/checke.png",
        "../Engine/assets/textures/checke.png",
        "../../assets/textures/checke.png",
        "../../Engine/assets/textures/checke.png",
        "../../../assets/textures/checke.png",
        "../../../Engine/assets/textures/checke.png",
    };
    for (const char* rel : candidates) {
        const fs::path p(rel);
        if (fs::exists(p))
            return p.lexically_normal().string();
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

} // namespace

int main() {
    std::printf("%s\n", eyelib::rendering::DescribeModule());
    std::printf("OpenEyeRenderingSandboxChecker: P6.11 validation scene\n");

    if (!glfwInit()) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window =
        glfwCreateWindow(static_cast<int>(kWidth), static_cast<int>(kHeight), "OpenEyeRenderingSandboxChecker", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: glfwCreateWindow failed\n");
        glfwTerminate();
        return 2;
    }

    std::unique_ptr<eyelib::rendering::rhi::IDevice> device =
        eyelib::rendering::rhi::CreateVulkanDevice(window, kWidth, kHeight);
    if (!device) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: CreateVulkanDevice failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }

    const std::string checkerPath = ResolveCheckerTexturePath();
    if (checkerPath.empty()) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: could not find assets/textures/checker.png\n");
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    std::shared_ptr<eyelib::rendering::Texture2D> checkerTex = std::make_shared<eyelib::rendering::Texture2D>();
    if (!checkerTex->LoadFromFile(device.get(), checkerPath, true, false) || !checkerTex->GetTextureRHI()) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: failed to load '%s'\n", checkerPath.c_str());
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 5;
    }

    const uint64_t handle64 = device->GetOrAllocateSharedBindlessTextureHandle(checkerTex->GetTextureRHI());
    if (handle64 == 0u || handle64 > 0xFFFFFFFFull) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: bindless handle allocation failed\n");
        checkerTex->Destroy(device.get());
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 6;
    }
    const uint32_t checkerBindless = static_cast<uint32_t>(handle64);
    std::printf("OpenEyeRenderingSandboxChecker: loaded checker '%s' bindless=%u\n", checkerPath.c_str(), checkerBindless);

    uint32_t altCheckerBindless = checkerBindless;
    std::shared_ptr<eyelib::rendering::Texture2D> altCheckerTex;
    const std::string altCheckerPath = ResolveAltCheckerTexturePath();
    if (!altCheckerPath.empty()) {
        altCheckerTex = std::make_shared<eyelib::rendering::Texture2D>();
        if (altCheckerTex->LoadFromFile(device.get(), altCheckerPath, true, false) && altCheckerTex->GetTextureRHI()) {
            const uint64_t altHandle64 = device->GetOrAllocateSharedBindlessTextureHandle(altCheckerTex->GetTextureRHI());
            if (altHandle64 != 0u && altHandle64 <= 0xFFFFFFFFull) {
                altCheckerBindless = static_cast<uint32_t>(altHandle64);
                std::printf("OpenEyeRenderingSandboxChecker: loaded alt checker '%s' bindless=%u\n", altCheckerPath.c_str(), altCheckerBindless);
            }
        }
    }

    const eyelib::rendering::MeshDescriptor meshDesc = MakeTexturedCubeMeshDescriptor();
    std::shared_ptr<eyelib::rendering::Mesh> cubeMesh = eyelib::rendering::Mesh::Create(device.get(), meshDesc);
    if (!cubeMesh) {
        std::fprintf(stderr, "OpenEyeRenderingSandboxChecker: Mesh::Create failed\n");
        checkerTex->Destroy(device.get());
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 7;
    }

    SandboxMeshResolver meshResolver{};
    meshResolver.mesh = cubeMesh.get();

    eyelib::rendering::RasterFeature rasterFeature;
    rasterFeature.SetMeshResolver(&meshResolver);
    eyelib::rendering::RenderMode activeMode = eyelib::rendering::RenderMode::Deferred;
    rasterFeature.SetRenderMode(activeMode);

    eyelib::rendering::RenderSceneView sceneView;
    {
        using namespace eyelib::rendering;
        sceneView.drawInstances.reserve(10);
        for (uint32_t i = 0; i < 10u; ++i) {
            SceneDrawInstance visible{};
            visible.meshHandle = 0u;
            visible.materialId = i;
            visible.transformIndex = i;
            visible.worldTransform = eyelib::math::Mat4(1.0f);
            visible.localBounds = {{-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}};
            visible.layer = DrawLayer::Opaque;
            sceneView.drawInstances.push_back(visible);
        }
    }

    eyelib::rendering::RenderWorld world(device.get());
    world.SetOutputSize(kWidth, kHeight);
    world.SetSceneView(&sceneView);
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    {
        using namespace eyelib::rendering::materials;
        // Case 0: scalar-only
        MaterialRecord scalarOnly = MakeDefaultMaterialRecord();
        scalarOnly.baseColorFactor = {0.95f, 0.1f, 0.1f, 1.0f};
        scalarOnly.metallicFactor = 0.0f;
        scalarOnly.roughnessFactor = 0.95f;
        world.Resources().MaterialTable().SetMaterial(0u, scalarOnly);

        // Case 1: full-texture (all known map slots populated).
        MaterialRecord fullTexture = MakeDefaultMaterialRecord();
        fullTexture.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
        fullTexture.baseColorTexture = checkerBindless;
        fullTexture.normalTexture = altCheckerBindless;
        fullTexture.metallicRoughnessTexture = checkerBindless;
        fullTexture.emissiveTexture = altCheckerBindless;
        fullTexture.occlusionTexture = checkerBindless;
        fullTexture.metallicFactor = 0.9f;
        fullTexture.roughnessFactor = 0.25f;
        fullTexture.emissiveFactor = {0.8f, 0.8f, 0.8f, 1.0f};
        world.Resources().MaterialTable().SetMaterial(1u, fullTexture);

        // Case 2: partial-texture (base color only).
        MaterialRecord partialTexture = MakeDefaultMaterialRecord();
        partialTexture.baseColorTexture = altCheckerBindless;
        partialTexture.baseColorFactor = {0.15f, 0.9f, 0.15f, 1.0f};
        partialTexture.metallicFactor = 0.0f;
        partialTexture.roughnessFactor = 1.0f;
        world.Resources().MaterialTable().SetMaterial(2u, partialTexture);

        // Case 3: missing normal map.
        MaterialRecord missingNormal = fullTexture;
        missingNormal.normalTexture = kInvalidMaterialTextureHandle;
        world.Resources().MaterialTable().SetMaterial(3u, missingNormal);

        // Case 4: missing metallic-roughness map.
        MaterialRecord missingMetalRough = fullTexture;
        missingMetalRough.metallicRoughnessTexture = kInvalidMaterialTextureHandle;
        missingMetalRough.metallicFactor = 1.0f;
        missingMetalRough.roughnessFactor = 0.05f;
        world.Resources().MaterialTable().SetMaterial(4u, missingMetalRough);

        // Case 5: missing emissive map.
        MaterialRecord missingEmissive = fullTexture;
        missingEmissive.emissiveTexture = kInvalidMaterialTextureHandle;
        missingEmissive.emissiveFactor = {0.0f, 0.0f, 0.0f, 1.0f};
        world.Resources().MaterialTable().SetMaterial(5u, missingEmissive);

        // Case 6: missing occlusion map.
        MaterialRecord missingOcclusion = fullTexture;
        missingOcclusion.occlusionTexture = kInvalidMaterialTextureHandle;
        missingOcclusion.occlusionStrength = 0.25f;
        world.Resources().MaterialTable().SetMaterial(6u, missingOcclusion);

        // Case 7: normal-map stress (very strong normal perturbation path).
        MaterialRecord normalStress = fullTexture;
        normalStress.normalTexture = checkerBindless;
        normalStress.normalScale = 3.0f;
        normalStress.roughnessFactor = 0.35f;
        world.Resources().MaterialTable().SetMaterial(7u, normalStress);

        // Case 8: metallic/roughness extreme (mirror-like conductor).
        MaterialRecord metalRoughExtreme = MakeDefaultMaterialRecord();
        metalRoughExtreme.baseColorFactor = {0.95f, 0.95f, 0.98f, 1.0f};
        metalRoughExtreme.metallicFactor = 1.0f;
        metalRoughExtreme.roughnessFactor = 0.04f;
        metalRoughExtreme.occlusionStrength = 1.0f;
        world.Resources().MaterialTable().SetMaterial(8u, metalRoughExtreme);

        // Case 9: emissive-only stress (light-emitting, near-black albedo).
        MaterialRecord emissiveOnly = MakeDefaultMaterialRecord();
        emissiveOnly.baseColorFactor = {0.02f, 0.02f, 0.02f, 1.0f};
        emissiveOnly.emissiveFactor = {4.0f, 1.0f, 0.2f, 1.0f};
        emissiveOnly.metallicFactor = 0.0f;
        emissiveOnly.roughnessFactor = 1.0f;
        world.Resources().MaterialTable().SetMaterial(9u, emissiveOnly);

        std::printf("OpenEyeRenderingSandboxChecker: validation cases loaded (0..9)\n");
        std::printf("  [0] scalar-only\n");
        std::printf("  [1] full-texture\n");
        std::printf("  [2] partial-texture\n");
        std::printf("  [3] missing normal\n");
        std::printf("  [4] missing metallic-roughness\n");
        std::printf("  [5] missing emissive\n");
        std::printf("  [6] missing occlusion\n");
        std::printf("  [7] normal-map stress\n");
        std::printf("  [8] metallic-roughness extreme\n");
        std::printf("  [9] emissive-only stress\n");
    }

    const std::vector<eyelib::math::Vec3> caseOffsets = {
        {-7.2f,  1.4f, 0.0f}, // scalar-only
        {-5.4f,  1.4f, 0.0f}, // full-texture
        {-3.6f,  1.4f, 0.0f}, // partial-texture
        {-1.8f,  1.4f, 0.0f}, // missing normal
        { 0.0f,  1.4f, 0.0f}, // missing metallic-roughness
        { 1.8f,  1.4f, 0.0f}, // missing emissive
        { 3.6f,  1.4f, 0.0f}, // missing occlusion
        {-1.8f, -1.4f, 0.0f}, // normal-map stress
        { 0.0f, -1.4f, 0.0f}, // metallic-roughness extreme
        { 1.8f, -1.4f, 0.0f}, // emissive-only stress
    };

    double lastT = glfwGetTime();
    float rotationAngle = 0.0f;
    bool autoToggleEnabled = true;
    double autoToggleTimer = 0.0;
    uint32_t autoToggleCount = 0;
    bool toggleKeyHeld = false;
    bool autoToggleKeyHeld = false;
    std::printf("OpenEyeRenderingSandboxChecker: mode=%s (TAB=toggle, F6=auto-toggle on/off)\n", ModeName(activeMode));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currentT = glfwGetTime();
        float dt = static_cast<float>(currentT - lastT);
        lastT = currentT;
        autoToggleTimer += dt;

        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!toggleKeyHeld) {
                activeMode = (activeMode == eyelib::rendering::RenderMode::Deferred)
                                 ? eyelib::rendering::RenderMode::Forward
                                 : eyelib::rendering::RenderMode::Deferred;
                rasterFeature.SetRenderMode(activeMode);
                std::printf("OpenEyeRenderingSandboxChecker: mode toggled -> %s\n", ModeName(activeMode));
            }
            toggleKeyHeld = true;
        } else {
            toggleKeyHeld = false;
        }

        if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS) {
            if (!autoToggleKeyHeld) {
                autoToggleEnabled = !autoToggleEnabled;
                std::printf("OpenEyeRenderingSandboxChecker: auto-toggle %s\n", autoToggleEnabled ? "ENABLED" : "DISABLED");
            }
            autoToggleKeyHeld = true;
        } else {
            autoToggleKeyHeld = false;
        }

        if (autoToggleEnabled && autoToggleTimer >= 1.0) {
            autoToggleTimer = 0.0;
            activeMode = (activeMode == eyelib::rendering::RenderMode::Deferred)
                             ? eyelib::rendering::RenderMode::Forward
                             : eyelib::rendering::RenderMode::Deferred;
            rasterFeature.SetRenderMode(activeMode);
            ++autoToggleCount;
            std::printf("OpenEyeRenderingSandboxChecker: auto-toggle #%u -> %s\n", autoToggleCount, ModeName(activeMode));
        }

        static float camX = 0.0f, camY = 1.2f, camZ = 5.0f;
        static float camYaw = 0.0f, camPitch = 0.0f;
        static double lastMouseX = 0.0, lastMouseY = 0.0;
        static bool firstMouse = true;

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (firstMouse) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstMouse = false;
            }
            float xoffset = static_cast<float>(mouseX - lastMouseX);
            float yoffset = static_cast<float>(lastMouseY - mouseY);
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            const float sensitivity = 0.1f;
            camYaw += xoffset * sensitivity;
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
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camX += front.x * velocity;
            camY += front.y * velocity;
            camZ += front.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camX -= front.x * velocity;
            camY -= front.y * velocity;
            camZ -= front.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camX -= right.x * velocity;
            camY -= right.y * velocity;
            camZ -= right.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camX += right.x * velocity;
            camY += right.y * velocity;
            camZ += right.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camY -= velocity;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camY += velocity;

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

        rotationAngle += dt * 0.8f;
        for (size_t i = 0; i < sceneView.drawInstances.size() && i < caseOffsets.size(); ++i) {
            const float phase = static_cast<float>(i) * 0.25f;
            const eyelib::math::Mat4 rot = glm::rotate(
                eyelib::math::Mat4(1.0f), rotationAngle + phase, eyelib::math::Vec3(0.0f, 1.0f, 0.0f));
            const eyelib::math::Mat4 tr = glm::translate(eyelib::math::Mat4(1.0f), caseOffsets[i]);
            const eyelib::math::Mat4 sc = glm::scale(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.75f));
            sceneView.drawInstances[i].worldTransform = tr * rot * sc;
        }

        device->BeginFrame();
        eyelib::rendering::rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (cmd)
            cmd->Begin();
        world.RunFrame(dt, cmd);
        device->EndFrame();
        device->Present();
    }

    // Ensure GPU work is complete before tearing down feature/device-owned query/pipeline resources.
    device->Finish();
    world.Shutdown();
    cubeMesh->Destroy(device.get());
    cubeMesh.reset();
    checkerTex->Destroy(device.get());
    checkerTex.reset();
    if (altCheckerTex) {
        altCheckerTex->Destroy(device.get());
        altCheckerTex.reset();
    }
    device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
