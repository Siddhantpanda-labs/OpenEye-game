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

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <memory>
#include <random>

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

eyelib::rendering::MeshDescriptor LoadMeshWithAssimp(const char* path) {
    using namespace eyelib::rendering;
    MeshDescriptor d{};
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
        
    if (!scene || !scene->mRootNode) {
        std::fprintf(stderr, "Assimp failed to load %s\n", path);
        return MakeCenterCubeMeshDescriptor(); // Fallback
    }
    
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* ai_mesh = scene->mMeshes[i];
        Submesh sm{};
        sm.indexOffset = static_cast<uint32_t>(d.indices32.size());
        sm.indexCount = ai_mesh->mNumFaces * 3;
        sm.materialIndex = ai_mesh->mMaterialIndex;
        
        uint32_t vertexBase = static_cast<uint32_t>(d.vertices.size());
        
        for (unsigned int v = 0; v < ai_mesh->mNumVertices; v++) {
            Vertex vert{};
            vert.position = { ai_mesh->mVertices[v].x, ai_mesh->mVertices[v].y, ai_mesh->mVertices[v].z };
            if (ai_mesh->mNormals)
                vert.normal = { ai_mesh->mNormals[v].x, ai_mesh->mNormals[v].y, ai_mesh->mNormals[v].z };
            if (ai_mesh->mTextureCoords[0])
                vert.uv = { ai_mesh->mTextureCoords[0][v].x, ai_mesh->mTextureCoords[0][v].y };
            d.vertices.push_back(vert);
        }
        
        for (unsigned int f = 0; f < ai_mesh->mNumFaces; f++) {
            aiFace face = ai_mesh->mFaces[f];
            for (unsigned int ind = 0; ind < face.mNumIndices; ind++) {
                d.indices32.push_back(vertexBase + face.mIndices[ind]);
            }
        }
        
        d.submeshes.push_back(sm);
    }
    
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

    std::shared_ptr<eyelib::rendering::Mesh> sandboxMesh = eyelib::rendering::Mesh::Create(device.get(), MakeCenterCubeMeshDescriptor());
    if (!sandboxMesh) {
        std::fprintf(stderr, "OpenEyeRenderingSandbox: centered cube Mesh::Create failed\n");
        device.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    SandboxMeshResolver meshResolver{};
    meshResolver.mesh = sandboxMesh.get();

    eyelib::rendering::RasterFeature rasterFeature;
    rasterFeature.SetMeshResolver(&meshResolver);
    rasterFeature.SetRenderMode(eyelib::rendering::RenderMode::Deferred);
    eyelib::rendering::RenderSceneView sceneView;

    {
        using namespace eyelib::rendering;
        SceneDrawInstance visible{};
        visible.meshHandle = 0u;
        visible.materialId = 3u; // Diagnostic yellow material in forward_opaque.frag
        visible.transformIndex = 0u;
        visible.worldTransform = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        visible.localBounds = {{-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}};
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
    world.TrySubscribeFeature(&rasterFeature);
    world.Initialize();

    double lastT = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currentT = glfwGetTime();
        float dt = static_cast<float>(currentT - lastT);
        lastT = currentT;

        static float camX = 0.0f, camY = 2.0f, camZ = 8.0f;
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
            camYaw -= xoffset * sensitivity; // Inverted yaw for right-click drag
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
        
        float velocity = 5.0f * dt;
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
        // Animated Cube
        eyelib::math::Mat4 model = glm::translate(eyelib::math::Mat4(1.0f), eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotationAngle, rotationAxis);
        sceneView.drawInstances[0].worldTransform = model;

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
