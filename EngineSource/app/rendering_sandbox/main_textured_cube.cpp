#include "rendering/RenderingModule.h"

#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/lighting/SceneLighting.h"
#include "eyelib/rendering/materials/MaterialSchema.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/RenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <memory>

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

eyelib::rendering::MeshDescriptor MakeCubeMeshDescriptor(float h) {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
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
    return d;
}

eyelib::rendering::MeshDescriptor MakePlaneMeshDescriptor(float halfExtent) {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    d.vertices.resize(4);
    d.indices32 = {0u, 1u, 2u, 2u, 3u, 0u};

    d.vertices[0].position = {-halfExtent, 0.0f, -halfExtent};
    d.vertices[1].position = {halfExtent, 0.0f, -halfExtent};
    d.vertices[2].position = {halfExtent, 0.0f, halfExtent};
    d.vertices[3].position = {-halfExtent, 0.0f, halfExtent};
    for (auto& v : d.vertices) {
        v.normal = {0.0f, 1.0f, 0.0f};
    }
    d.vertices[0].uv = {0.0f, 0.0f};
    d.vertices[1].uv = {10.0f, 0.0f};
    d.vertices[2].uv = {10.0f, 10.0f};
    d.vertices[3].uv = {0.0f, 10.0f};
    return d;
}

eyelib::rendering::MeshDescriptor MakeUvSphereMeshDescriptor(float radius, uint32_t stacks, uint32_t slices) {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    d.vertices.reserve((stacks + 1u) * (slices + 1u));
    d.indices32.reserve(stacks * slices * 6u);

    for (uint32_t i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = v * 3.14159265358979323846f;
        for (uint32_t j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(slices);
            const float theta = u * 6.28318530717958647692f;

            const float x = std::sin(phi) * std::cos(theta);
            const float y = std::cos(phi);
            const float z = std::sin(phi) * std::sin(theta);

            Vertex vert{};
            vert.position = {x * radius, y * radius, z * radius};
            vert.normal = {x, y, z};
            vert.uv = {u, v};
            d.vertices.push_back(vert);
        }
    }

    const uint32_t row = slices + 1u;
    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < slices; ++j) {
            const uint32_t a = i * row + j;
            const uint32_t b = a + row;
            d.indices32.push_back(a);
            d.indices32.push_back(b);
            d.indices32.push_back(a + 1u);
            d.indices32.push_back(a + 1u);
            d.indices32.push_back(b);
            d.indices32.push_back(b + 1u);
        }
    }
    return d;
}

struct SandboxMeshResolver final : eyelib::rendering::IMeshHandleResolver {
    eyelib::rendering::Mesh* plane = nullptr;
    eyelib::rendering::Mesh* sphere = nullptr;
    eyelib::rendering::Mesh* cube = nullptr;

    eyelib::rendering::MeshHandle TryResolveMesh(uint32_t meshHandle) const noexcept override {
        switch (meshHandle) {
        case 0u: return plane ? plane->GetHandle() : eyelib::rendering::MeshHandle{};
        case 1u: return sphere ? sphere->GetHandle() : eyelib::rendering::MeshHandle{};
        case 2u: return cube ? cube->GetHandle() : eyelib::rendering::MeshHandle{};
        default: return {};
        }
    }
};

} // namespace

int main() {
    std::printf("%s\n", eyelib::rendering::DescribeModule());
    std::printf("RenderingSandbox: simple shadow scene\n");

    if (!glfwInit()) {
        std::fprintf(stderr, "RenderingSandbox: glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window =
        glfwCreateWindow(static_cast<int>(kWidth), static_cast<int>(kHeight), "RenderingSandbox", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "RenderingSandbox: glfwCreateWindow failed\n");
        glfwTerminate();
        return 2;
    }

    std::unique_ptr<eyelib::rendering::rhi::IDevice> device =
        eyelib::rendering::rhi::CreateVulkanDevice(window, kWidth, kHeight);
    if (!device) {
        std::fprintf(stderr, "RenderingSandbox: CreateVulkanDevice failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }

    std::shared_ptr<eyelib::rendering::Mesh> planeMesh =
        eyelib::rendering::Mesh::Create(device.get(), MakePlaneMeshDescriptor(25.0f));
    std::shared_ptr<eyelib::rendering::Mesh> sphereMesh =
        eyelib::rendering::Mesh::Create(device.get(), MakeUvSphereMeshDescriptor(3.75f, 24u, 32u));
    std::shared_ptr<eyelib::rendering::Mesh> cubeMesh =
        eyelib::rendering::Mesh::Create(device.get(), MakeCubeMeshDescriptor(2.5f));

    if (!planeMesh || !sphereMesh || !cubeMesh) {
        std::fprintf(stderr, "RenderingSandbox: mesh creation failed\n");
        if (planeMesh) planeMesh->Destroy(device.get());
        if (sphereMesh) sphereMesh->Destroy(device.get());
        if (cubeMesh) cubeMesh->Destroy(device.get());
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    SandboxMeshResolver meshResolver{};
    meshResolver.plane = planeMesh.get();
    meshResolver.sphere = sphereMesh.get();
    meshResolver.cube = cubeMesh.get();

    eyelib::rendering::RasterFeature rasterFeature;
    eyelib::rendering::ShadowFeature shadowFeature;
    rasterFeature.SetMeshResolver(&meshResolver);
    shadowFeature.SetMeshResolver(&meshResolver);
    rasterFeature.SetRenderMode(eyelib::rendering::RenderMode::Deferred);

    // Directional light disabled — scene lit exclusively by spot light below.
    eyelib::rendering::SceneLighting::SetDirectionalLight({0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f);

    // Light-blue downward spot light centered high above the scene.
    {
        eyelib::rendering::SceneLighting::ClearSpotLights();
        eyelib::rendering::SceneLighting::SpotLightSceneState spot{};
        spot.position      = {0.0f, 18.0f, 0.0f};    // High above scene center
        spot.direction     = {0.0f, -1.0f, 0.0f};    // Straight down
        spot.color         = {1.0f, 0.85f, 0.1f};    // Bright yellow
        spot.intensity     = 200.0f;                  // Very bright
        spot.range         = 35.0f;                   // Covers ground plane
        spot.innerConeCos  = std::cos(glm::radians(20.0f)); // Narrower hot center (lower radius)
        spot.outerConeCos  = std::cos(glm::radians(35.0f)); // Tighter edge
        spot.castShadows   = true;
        eyelib::rendering::SceneLighting::AddSpotLight(spot);
    }

    eyelib::rendering::RenderSceneView sceneView;
    sceneView.drawInstances.reserve(3);
    {
        using namespace eyelib::rendering;
        SceneDrawInstance plane{};
        plane.meshHandle = 0u;
        plane.materialId = 0u;
        plane.transformIndex = 0u;
        plane.worldTransform = eyelib::math::Mat4(1.0f);
        plane.localBounds = {{-25.0f, -0.01f, -25.0f}, {25.0f, 0.01f, 25.0f}};
        plane.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(plane);

        SceneDrawInstance sphere{};
        sphere.meshHandle = 1u;
        sphere.materialId = 1u;
        sphere.transformIndex = 1u;
        sphere.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(-3.5f, 6.5f, 0.0f));
        sphere.localBounds = {{-3.75f, -3.75f, -3.75f}, {3.75f, 3.75f, 3.75f}};
        sphere.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(sphere);

        SceneDrawInstance cube{};
        cube.meshHandle = 2u;
        cube.materialId = 2u;
        cube.transformIndex = 2u;
        cube.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(3.5f, 5.0f, 0.0f));
        cube.localBounds = {{-2.5f, -2.5f, -2.5f}, {2.5f, 2.5f, 2.5f}};
        cube.layer = DrawLayer::Opaque;
        sceneView.drawInstances.push_back(cube);
    }

    eyelib::rendering::RenderWorld world(device.get());
    world.SetOutputSize(kWidth, kHeight);
    world.SetSceneView(&sceneView);
    world.TrySubscribeFeature(&shadowFeature);
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    {
        using namespace eyelib::rendering::materials;
        MaterialRecord planeMat = MakeDefaultMaterialRecord();
        planeMat.baseColorFactor = {0.35f, 0.45f, 0.35f, 1.0f}; // dull green
        planeMat.metallicFactor = 0.0f;
        planeMat.roughnessFactor = 0.95f;
        world.Resources().MaterialTable().SetMaterial(0u, planeMat);

        MaterialRecord sphereMat = MakeDefaultMaterialRecord();
        sphereMat.baseColorFactor = {0.12f, 0.32f, 0.88f, 1.0f}; // blue
        sphereMat.metallicFactor = 0.0f;
        sphereMat.roughnessFactor = 0.45f;
        world.Resources().MaterialTable().SetMaterial(1u, sphereMat);

        MaterialRecord cubeMat = MakeDefaultMaterialRecord();
        cubeMat.baseColorFactor = {0.88f, 0.16f, 0.16f, 1.0f}; // red
        cubeMat.metallicFactor = 0.0f;
        cubeMat.roughnessFactor = 0.55f;
        world.Resources().MaterialTable().SetMaterial(2u, cubeMat);
    }

    std::printf("RenderingSandbox: scene loaded (plane=green, sphere=blue, cube=red)\n");
    std::printf("RenderingSandbox: light-blue downward spot light, no directional light\n");

    double lastT = glfwGetTime();
    static float camX = 0.0f, camY = 2.3f, camZ = 7.0f;
    static float camYaw = -90.0f, camPitch = -20.0f;
    static double lastMouseX = 0.0, lastMouseY = 0.0;
    static bool firstMouse = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double currentT = glfwGetTime();
        const float dt = static_cast<float>(currentT - lastT);
        lastT = currentT;

        // Rotate the cube around a diagonal axis for easier shadow stability checks.
        {
            const float angle = static_cast<float>(currentT) * 0.9f;
            const eyelib::math::Vec3 axis = glm::normalize(eyelib::math::Vec3(1.0f, 1.0f, 0.45f));
            const eyelib::math::Mat4 rotation = glm::rotate(eyelib::math::Mat4(1.0f), angle, axis);
            const eyelib::math::Mat4 translation =
                glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(3.5f, 5.0f, 0.0f));
            sceneView.drawInstances[2].worldTransform = translation * rotation;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (firstMouse) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstMouse = false;
            }
            const float xoffset = static_cast<float>(mouseX - lastMouseX);
            const float yoffset = static_cast<float>(lastMouseY - mouseY);
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

        float velocity = 6.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) velocity *= 3.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camX += front.x * velocity; camY += front.y * velocity; camZ += front.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camX -= front.x * velocity; camY -= front.y * velocity; camZ -= front.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camX -= right.x * velocity; camY -= right.y * velocity; camZ -= right.z * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camX += right.x * velocity; camY += right.y * velocity; camZ += right.z * velocity;
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

        device->BeginFrame();
        eyelib::rendering::rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (cmd) cmd->Begin();
        world.RunFrame(dt, cmd);
        device->EndFrame();
        device->Present();
    }

    device->Finish();
    world.Shutdown();
    planeMesh->Destroy(device.get());
    sphereMesh->Destroy(device.get());
    cubeMesh->Destroy(device.get());
    planeMesh.reset();
    sphereMesh.reset();
    cubeMesh.reset();
    device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
