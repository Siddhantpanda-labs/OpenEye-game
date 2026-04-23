// Standalone harness: EyelibRendering + GLFW + Vulkan device (no api/render, no EyelibRender).
#include "rendering/RenderingModule.h"

#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/RenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <memory>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

eyelib::rendering::MeshDescriptor MakeCenterQuadMeshDescriptor() {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    const float hx = 0.6f;
    const float hy = 0.6f;
    const eyelib::math::Vec3 n{0.0f, 0.0f, 1.0f};
    d.vertices.reserve(4);
    d.indices32.reserve(6);

    Vertex v0{}; v0.position = {-hx, -hy, 0.0f}; v0.normal = n; v0.uv = {0.0f, 0.0f};
    Vertex v1{}; v1.position = { hx, -hy, 0.0f}; v1.normal = n; v1.uv = {1.0f, 0.0f};
    Vertex v2{}; v2.position = { hx,  hy, 0.0f}; v2.normal = n; v2.uv = {1.0f, 1.0f};
    Vertex v3{}; v3.position = {-hx,  hy, 0.0f}; v3.normal = n; v3.uv = {0.0f, 1.0f};
    d.vertices.push_back(v0);
    d.vertices.push_back(v1);
    d.vertices.push_back(v2);
    d.vertices.push_back(v3);
    d.indices32 = {0, 1, 2, 2, 3, 0};

    d.submeshes.push_back(Submesh{0u, 6u, 0});
    return d;
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

    std::shared_ptr<eyelib::rendering::Mesh> sandboxMesh = eyelib::rendering::Mesh::Create(device.get(), MakeCenterQuadMeshDescriptor());
    if (!sandboxMesh) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: centered quad Mesh::Create failed\n");
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    SandboxMeshResolver meshResolver{};
    meshResolver.mesh = sandboxMesh.get();

    eyelib::rendering::RasterFeature rasterFeature;
    rasterFeature.SetMeshResolver(&meshResolver);
    eyelib::rendering::RenderSceneView sceneView;

    {
        using namespace eyelib::rendering;
        SceneDrawInstance visible{};
        visible.meshHandle = 0u;
        visible.materialId = 3u; // Diagnostic yellow material in forward_opaque.frag
        visible.transformIndex = 0u;
        visible.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        visible.localBounds = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
        visible.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(visible);
    }

    eyelib::rendering::RenderWorld world(device.get());
    world.SetOutputSize(kWidth, kHeight);
    world.SetSceneView(&sceneView);
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    double lastT = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int frameW = 0;
        int frameH = 0;
        glfwGetFramebufferSize(window, &frameW, &frameH);
        if (frameW > 0 && frameH > 0) {
            world.SetOutputSize(static_cast<uint32_t>(frameW), static_cast<uint32_t>(frameH));
            device->OnWindowResized(static_cast<uint32_t>(frameW), static_cast<uint32_t>(frameH));

            eyelib::rendering::camera::FreeFlyCameraDesc camDesc{};
            camDesc.position = {0.0f, 2.0f, 8.0f};
            camDesc.yaw = 0.0f;
            camDesc.pitch = 0.0f;
            camDesc.fovDegrees = 60.0f;
            camDesc.nearPlane = 0.1f;
            camDesc.farPlane = 1000.0f;
            sceneView.camera = eyelib::rendering::camera::BuildFreeFlyCameraView(
                camDesc, static_cast<float>(frameW), static_cast<float>(frameH));
            sceneView.camera = eyelib::rendering::camera::AdjustCameraForBackend(
                sceneView.camera, eyelib::rendering::RenderBackend::Vulkan);
        }

        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastT);
        lastT = now;
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
