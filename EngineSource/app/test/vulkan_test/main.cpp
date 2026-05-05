/**
 * Vulkan RHI-only test app: manually render a rotating cube.
 */

#include "api/core/All.h"
#include "api/platform/Window.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/platform/window/Window.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIUtils.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace rhi = eyelib::render::rhi;

namespace {
constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

struct VertexPC {
    float px, py, pz;
    float nx, ny, nz;
};

struct PushData {
    eyelib::math::Mat4 mvp{1.0f};
    eyelib::math::Mat4 model{1.0f};
};

static std::string ReadTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool LoadShaderSources(std::string& outVert, std::string& outFrag) {
    const std::string root = eyelib::platform::filesystem::GetEngineAssetsRoot();
    const std::string base = root.empty()
        ? std::string("assets/shaders/vulkan/tests/rhi_cube")
        : eyelib::platform::filesystem::NormalizePath(root + "/assets/shaders/vulkan/tests/rhi_cube");
    outVert = ReadTextFile(base + ".vert");
    outFrag = ReadTextFile(base + ".frag");
    return !outVert.empty() && !outFrag.empty();
}

static eyelib::math::Mat4 RotationY(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    eyelib::math::Mat4 m(1.0f);
    m[0][0] = c;
    m[0][2] = s;
    m[2][0] = -s;
    m[2][2] = c;
    return m;
}

static eyelib::math::Mat4 VulkanProjection(float fovYRadians, float aspect, float nearZ, float farZ) {
    eyelib::math::Mat4 proj = eyelib::math::Perspective(fovYRadians, aspect, nearZ, farZ);
    eyelib::math::Mat4 fix(1.0f);
    fix[2][2] *= 0.5f;
    fix[3][2] += 0.5f;
    return fix * proj;
}
} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    eyelib::api::core::Engine::Config engineConfig{};
    engineConfig.logFile = "VulkanTest.log";
    if (!eyelib::api::core::Engine::Initialize(engineConfig)) {
        std::cerr << "VulkanTest: Engine init failed\n";
        return 2;
    }

    eyelib::api::Window window;
    eyelib::api::WindowConfig winConfig{};
    winConfig.title = "Vulkan Test (RHI manual cube)";
    winConfig.width = static_cast<int>(kWidth);
    winConfig.height = static_cast<int>(kHeight);
    winConfig.useNativeApiOnly = true;
    if (!window.Initialize(winConfig)) {
        std::cerr << "VulkanTest: Window init failed\n";
        eyelib::api::core::Engine::Shutdown();
        return 3;
    }

    auto* platformWindow = static_cast<eyelib::platform::Window*>(window.GetPlatformWindow());
    if (!platformWindow) {
        std::cerr << "VulkanTest: platform window missing\n";
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 4;
    }

    std::unique_ptr<rhi::IDevice> deviceHolder =
        eyelib::platform::CreateGraphicsDevice(platformWindow, kWidth, kHeight, eyelib::render::RenderBackend::Vulkan);
    if (!deviceHolder) {
        std::cerr << "VulkanTest: CreateGraphicsDevice(Vulkan) failed\n";
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 5;
    }
    rhi::IDevice* device = deviceHolder.get();

    std::string vsSource;
    std::string fsSource;
    if (!LoadShaderSources(vsSource, fsSource)) {
        std::cerr << "VulkanTest: failed to load shader files assets/shaders/vulkan/tests/rhi_cube.(vert|frag)\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 6;
    }

    rhi::IShader* vs = device->CreateShader(rhi::ShaderStage::Vertex, vsSource.c_str(), vsSource.size());
    rhi::IShader* fs = device->CreateShader(rhi::ShaderStage::Fragment, fsSource.c_str(), fsSource.size());
    if (!vs || !fs) {
        std::cerr << "VulkanTest: shader compile failed\n";
        if (vs) device->DestroyShader(vs);
        if (fs) device->DestroyShader(fs);
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 7;
    }

    rhi::VertexLayout layout{};
    layout.strideInBytes = sizeof(VertexPC);
    layout.attributes.push_back({0u, rhi::VertexAttributeType::Float3, 0u});
    layout.attributes.push_back({1u, rhi::VertexAttributeType::Float3, 12u});

    rhi::PipelineDesc pipeDesc{};
    pipeDesc.vertexShader = vs;
    pipeDesc.fragmentShader = fs;
    pipeDesc.vertexLayout = layout;
    pipeDesc.topology = rhi::PrimitiveTopology::TriangleList;
    pipeDesc.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    pipeDesc.depthStencil.depthTest = true;
    pipeDesc.depthStencil.depthWrite = true;
    pipeDesc.attachmentFormats = rhi::PipelineAttachmentFormats{{{rhi::RHITextureFormat::BGRA8}}, rhi::RHITextureFormat::Depth24};

    rhi::IPipeline* pipeline = device->CreateGraphicsPipeline(pipeDesc);
    device->DestroyShader(vs);
    device->DestroyShader(fs);
    if (!pipeline) {
        std::cerr << "VulkanTest: graphics pipeline creation failed\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 8;
    }

    const std::vector<VertexPC> vertices = {
        // Front (+Z)
        {-1.f, -1.f,  1.f,  0.f,  0.f,  1.f}, { 1.f, -1.f,  1.f,  0.f,  0.f,  1.f}, { 1.f,  1.f,  1.f,  0.f,  0.f,  1.f}, {-1.f,  1.f,  1.f,  0.f,  0.f,  1.f},
        // Back (-Z)
        { 1.f, -1.f, -1.f,  0.f,  0.f, -1.f}, {-1.f, -1.f, -1.f,  0.f,  0.f, -1.f}, {-1.f,  1.f, -1.f,  0.f,  0.f, -1.f}, { 1.f,  1.f, -1.f,  0.f,  0.f, -1.f},
        // Left (-X)
        {-1.f, -1.f, -1.f, -1.f,  0.f,  0.f}, {-1.f, -1.f,  1.f, -1.f,  0.f,  0.f}, {-1.f,  1.f,  1.f, -1.f,  0.f,  0.f}, {-1.f,  1.f, -1.f, -1.f,  0.f,  0.f},
        // Right (+X)
        { 1.f, -1.f,  1.f,  1.f,  0.f,  0.f}, { 1.f, -1.f, -1.f,  1.f,  0.f,  0.f}, { 1.f,  1.f, -1.f,  1.f,  0.f,  0.f}, { 1.f,  1.f,  1.f,  1.f,  0.f,  0.f},
        // Top (+Y)
        {-1.f,  1.f,  1.f,  0.f,  1.f,  0.f}, { 1.f,  1.f,  1.f,  0.f,  1.f,  0.f}, { 1.f,  1.f, -1.f,  0.f,  1.f,  0.f}, {-1.f,  1.f, -1.f,  0.f,  1.f,  0.f},
        // Bottom (-Y)
        {-1.f, -1.f, -1.f,  0.f, -1.f,  0.f}, { 1.f, -1.f, -1.f,  0.f, -1.f,  0.f}, { 1.f, -1.f,  1.f,  0.f, -1.f,  0.f}, {-1.f, -1.f,  1.f,  0.f, -1.f,  0.f},
    };
    const std::vector<uint32_t> indices = {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23
    };

    rhi::BufferDesc vbDesc{};
    vbDesc.sizeInBytes = vertices.size() * sizeof(VertexPC);
    vbDesc.usage = rhi::BufferUsage::Vertex;
    vbDesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    rhi::IBuffer* vbo = device->CreateBuffer(vbDesc, vertices.data());

    rhi::BufferDesc ibDesc{};
    ibDesc.sizeInBytes = indices.size() * sizeof(uint32_t);
    ibDesc.usage = rhi::BufferUsage::Index;
    ibDesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    rhi::IBuffer* ibo = device->CreateBuffer(ibDesc, indices.data());

    if (!vbo || !ibo) {
        std::cerr << "VulkanTest: buffer creation failed\n";
        if (vbo) device->DestroyBuffer(vbo);
        if (ibo) device->DestroyBuffer(ibo);
        device->DestroyPipeline(pipeline);
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 9;
    }

    std::vector<rhi::RHITextureFormat> colorFormats = { rhi::RHITextureFormat::BGRA8 };
    rhi::IRenderTarget* offscreenRT = device->CreateRenderTarget(kWidth, kHeight, colorFormats, rhi::RHITextureFormat::Depth24, 1, false);
    if (!offscreenRT) {
        std::cerr << "VulkanTest: offscreen render target creation failed\n";
        device->DestroyBuffer(vbo);
        device->DestroyBuffer(ibo);
        device->DestroyPipeline(pipeline);
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 10;
    }

    std::cout << "VulkanTest: RHI manual cube running.\n";
    const auto t0 = std::chrono::steady_clock::now();
    while (!window.ShouldClose()) {
        window.PollEvents();

        device->BeginFrame();
        rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (!cmd) {
            device->EndFrame();
            device->Present();
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const float t = std::chrono::duration<float>(now - t0).count();
        const eyelib::math::Mat4 model = RotationY(t);
        const eyelib::math::Mat4 view = eyelib::math::LookAt(
            eyelib::math::Vec3(0.0f, 0.0f, 5.0f),
            eyelib::math::Vec3(0.0f, 0.0f, 0.0f),
            eyelib::math::Vec3(0.0f, 1.0f, 0.0f));
        const eyelib::math::Mat4 proj = VulkanProjection(60.0f * 3.14159265f / 180.0f, static_cast<float>(kWidth) / static_cast<float>(kHeight), 0.1f, 100.0f);
        PushData push{};
        push.mvp = proj * view * model;
        push.model = model;

        rhi::ClearAndSetRenderTargetWithViewport(cmd, offscreenRT, 0, 0, kWidth, kHeight, {0.06f, 0.08f, 0.12f, 1.0f});
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        cmd->BindPipeline(pipeline);
        cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(PushData)), &push);
        cmd->BindVertexBuffer(vbo, 0, 0);
        cmd->BindIndexBuffer(ibo, rhi::IndexType::UInt32, 0);
        cmd->DrawIndexed(static_cast<uint32_t>(indices.size()));

        device->BlitToDefaultFramebuffer(offscreenRT);
        device->EndFrame();
        device->Present();
    }

    device->DestroyRenderTarget(offscreenRT);
    device->DestroyBuffer(vbo);
    device->DestroyBuffer(ibo);
    device->DestroyPipeline(pipeline);
    deviceHolder.reset();
    window.Shutdown();
    eyelib::api::core::Engine::Shutdown();
    return 0;
}
