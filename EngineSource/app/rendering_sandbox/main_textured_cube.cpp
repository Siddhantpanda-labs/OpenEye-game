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

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

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

    d.submeshes.push_back(Submesh{0u, static_cast<uint32_t>(d.indices32.size()), 0});
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
    std::printf("OpenEyeRenderingSandboxChecker: textured rotating cube\n");

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
    rasterFeature.SetRenderMode(eyelib::rendering::RenderMode::Deferred);

    eyelib::rendering::RenderSceneView sceneView;
    {
        using namespace eyelib::rendering;
        SceneDrawInstance visible{};
        visible.meshHandle = 0u;
        visible.materialId = 0u;
        visible.transformIndex = 0u;
        visible.worldTransform = eyelib::math::Mat4(1.0f);
        visible.localBounds = {{-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}};
        visible.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(visible);
    }

    eyelib::rendering::RenderWorld world(device.get());
    world.SetOutputSize(kWidth, kHeight);
    world.SetSceneView(&sceneView);
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    {
        using namespace eyelib::rendering::materials;
        MaterialRecord mat = MakeDefaultMaterialRecord();
        mat.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
        mat.baseColorTexture = checkerBindless;
        world.Resources().MaterialTable().SetMaterial(0u, mat);
    }

    double lastT = glfwGetTime();
    float rotationAngle = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currentT = glfwGetTime();
        float dt = static_cast<float>(currentT - lastT);
        lastT = currentT;

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
        const eyelib::math::Mat4 rot =
            glm::rotate(eyelib::math::Mat4(1.0f), rotationAngle, eyelib::math::Vec3(0.0f, 1.0f, 0.0f));
        sceneView.drawInstances[0].worldTransform = rot * glm::scale(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.85f));

        device->BeginFrame();
        eyelib::rendering::rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (cmd)
            cmd->Begin();
        world.RunFrame(dt, cmd);
        device->EndFrame();
        device->Present();
    }

    world.Shutdown();
    cubeMesh->Destroy(device.get());
    cubeMesh.reset();
    checkerTex->Destroy(device.get());
    checkerTex.reset();
    device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
