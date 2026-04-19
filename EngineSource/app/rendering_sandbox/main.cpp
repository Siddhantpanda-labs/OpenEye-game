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

eyelib::rendering::MeshDescriptor MakeUnitCubeMeshDescriptor() {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    const float h = 0.5f;
    const std::array<eyelib::math::Vec3, 8> p = {{
        {-h, -h, -h},
        {h, -h, -h},
        {h, h, -h},
        {-h, h, -h},
        {-h, -h, h},
        {h, -h, h},
        {h, h, h},
        {-h, h, h},
    }};
    d.vertices.reserve(8);
    for (const eyelib::math::Vec3& pos : p) {
        Vertex v{};
        v.position = pos;
        v.normal = glm::normalize(pos);
        d.vertices.push_back(v);
    }
    d.indices32 = {
        0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4,
    };
    d.submeshes.push_back(Submesh{0u, static_cast<uint32_t>(d.indices32.size()), 0});
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

    std::shared_ptr<eyelib::rendering::Mesh> sandboxMesh = eyelib::rendering::Mesh::Create(device.get(), MakeUnitCubeMeshDescriptor());
    if (!sandboxMesh) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: unit cube Mesh::Create failed\n");
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
        visible.materialId = 1u;
        visible.transformIndex = 0u;
        visible.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        visible.localBounds = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
        visible.layer = DrawLayer::Opaque;

        SceneDrawInstance culled{};
        culled.meshHandle = 0u;
        culled.materialId = 2u;
        culled.transformIndex = 1u;
        // Behind camera (+Z); frustum looks toward -Z from (0,2,8).
        culled.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 2.0f, 50.0f));
        culled.localBounds = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
        culled.layer = DrawLayer::Opaque;

        sceneView.drawInstances.push_back(visible);
        sceneView.drawInstances.push_back(culled);
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
