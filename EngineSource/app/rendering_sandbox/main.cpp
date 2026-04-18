// Standalone harness: EyelibRendering + GLFW + Vulkan device (no api/render, no EyelibRender).
#include "rendering/RenderingModule.h"

#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/RenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <memory>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

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

    eyelib::rendering::RasterFeature rasterFeature;
    eyelib::rendering::RenderSceneView sceneView;

    {
        using namespace eyelib::rendering;
        SceneDrawInstance visible{};
        visible.meshHandle = 1u;
        visible.materialId = 10u;
        visible.transformIndex = 0u;
        visible.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        visible.localBounds = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
        visible.layer = DrawLayer::Opaque;

        SceneDrawInstance culled{};
        culled.meshHandle = 2u;
        culled.materialId = 11u;
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

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        if (w > 0 && h > 0) {
            world.SetOutputSize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            device->OnWindowResized(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

            eyelib::rendering::camera::FreeFlyCameraDesc camDesc{};
            camDesc.position = {0.0f, 2.0f, 8.0f};
            camDesc.yaw = 0.0f;
            camDesc.pitch = 0.0f;
            camDesc.fovDegrees = 60.0f;
            camDesc.nearPlane = 0.1f;
            camDesc.farPlane = 1000.0f;
            sceneView.camera = eyelib::rendering::camera::BuildFreeFlyCameraView(
                camDesc, static_cast<float>(w), static_cast<float>(h));
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
    device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
