/**
 * Standalone Diligent test: window + device, draw a rotating cube with test/cube
 * (Constants: u_Model, u_View, u_Projection). Verifies: pipeline, VB/IB, SetUniformMat4, DrawIndexed.
 *
 * Build with OPENEYE_USE_DILIGENT=ON. Run from build/bin/Debug (or ensure
 * assets/shaders/diligent/test/ is available next to exe or cwd).
 */
#include "eyelib/platform/window/Window.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/Types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <cstdio>
#include <chrono>

int main() {
    using namespace eyelib::platform;
    using namespace eyelib::render;
    using namespace eyelib::render::rhi;

    eyelib::core::Log::Info("DiligentTest: initializing window (useNativeApiOnly for Vulkan)");
    Window window;
    WindowConfig config;
    config.title = "Diligent Test - Rotating Cube";
    config.width = 640;
    config.height = 480;
    config.useNativeApiOnly = true;
    if (!window.Initialize(config)) {
        eyelib::core::Log::Error("DiligentTest: window init failed");
        fprintf(stderr, "DiligentTest: Window::Initialize failed.\n");
        return 1;
    }

    eyelib::core::Log::Info("DiligentTest: creating Diligent graphics device");
    auto device = CreateGraphicsDevice(&window, static_cast<uint32_t>(config.width), static_cast<uint32_t>(config.height), RenderBackend::Diligent);
    if (!device) {
        eyelib::core::Log::Error("DiligentTest: CreateGraphicsDevice(Diligent) returned null");
        fprintf(stderr, "DiligentTest: CreateGraphicsDevice failed.\n");
        window.Shutdown();
        return 1;
    }

    // Cube pipeline: vertex layout = position float3 + normal float3 (for lighting)
    VertexLayout cubeLayout;
    cubeLayout.strideInBytes = 24;
    cubeLayout.attributes = {{0, VertexAttributeType::Float3, 0}, {1, VertexAttributeType::Float3, 12}};

    eyelib::core::Log::Info("DiligentTest: creating test/solid and test/cube pipelines");
    VertexLayout emptyLayout;
    IPipeline* solidPipeline = device->CreateGraphicsPipelineFromName("test/solid", emptyLayout, PrimitiveTopology::TriangleList);
    IPipeline* cubePipeline = device->CreateGraphicsPipelineFromName("test/cube", cubeLayout, PrimitiveTopology::TriangleList);
    if (!solidPipeline || !cubePipeline) {
        eyelib::core::Log::ErrorF("DiligentTest: CreateGraphicsPipelineFromName failed (solid=%p cube=%p)", (void*)solidPipeline, (void*)cubePipeline);
        device.reset();
        window.Shutdown();
        return 1;
    }

    // Fullscreen quad for test/solid (green)
    struct PosTex { float x, y, u, v; };
    const PosTex quadVerts[] = {{-1.f, -1.f, 0.f, 1.f}, {1.f, -1.f, 1.f, 1.f}, {-1.f, 1.f, 0.f, 0.f}, {1.f, 1.f, 1.f, 0.f}};
    const uint16_t quadIndices[] = {0, 1, 2, 1, 3, 2};
    BufferDesc qvbDesc = {sizeof(quadVerts), BufferUsage::Vertex, BufferMemoryHint::DeviceOnly};
    IBuffer* quadVB = device->CreateBuffer(qvbDesc, quadVerts);
    BufferDesc qibDesc = {sizeof(quadIndices), BufferUsage::Index, BufferMemoryHint::DeviceOnly};
    IBuffer* quadIB = device->CreateBuffer(qibDesc, quadIndices);
    std::vector<RHITextureFormat> oneColor = {RHITextureFormat::RGBA8};
    IRenderTarget* rtDummy = device->CreateRenderTarget(1, 1, oneColor, RHITextureFormat::None);
    ITexture* dummyTex = rtDummy ? rtDummy->GetColorTexture(0) : nullptr;
    if (!quadVB || !quadIB || !dummyTex) {
        eyelib::core::Log::Error("DiligentTest: quad VB/IB or dummy tex failed");
        if (quadVB) device->DestroyBuffer(quadVB);
        if (quadIB) device->DestroyBuffer(quadIB);
        if (rtDummy) device->DestroyRenderTarget(rtDummy);
        device->DestroyPipeline(solidPipeline);
        device->DestroyPipeline(cubePipeline);
        device.reset();
        window.Shutdown();
        return 1;
    }

    // Cube: 24 vertices (4 per face), each position (float3) + normal (float3) = 6 floats. Stride 24.
    // Order: back(z-), front(z+), right(x+), left(x-), top(y+), bottom(y-). Normals point outward.
    const float cubeVerts[] = {
        -1,-1,-1,  0, 0,-1,   1,-1,-1,  0, 0,-1,   1, 1,-1,  0, 0,-1,  -1, 1,-1,  0, 0,-1,
         1,-1, 1,  0, 0, 1,  -1,-1, 1,  0, 0, 1,  -1, 1, 1,  0, 0, 1,   1, 1, 1,  0, 0, 1,
         1,-1, 1,  1, 0, 0,   1,-1,-1,  1, 0, 0,   1, 1,-1,  1, 0, 0,   1, 1, 1,  1, 0, 0,
        -1,-1,-1, -1, 0, 0,  -1,-1, 1, -1, 0, 0,  -1, 1, 1, -1, 0, 0,  -1, 1,-1, -1, 0, 0,
        -1, 1, 1,  0, 1, 0,   1, 1, 1,  0, 1, 0,   1, 1,-1,  0, 1, 0,  -1, 1,-1,  0, 1, 0,
        -1,-1,-1,  0,-1, 0,   1,-1,-1,  0,-1, 0,   1,-1, 1,  0,-1, 0,  -1,-1, 1,  0,-1, 0,
    };
    // Winding: CW when viewed from outside (FrontCounterClockwise=false, D3D default).
    // Each face: swap middle vertex so triangle is CW from outside.
    const uint16_t cubeIndices[] = {
        0, 2, 1,  0, 3, 2,  4, 6, 5,  4, 7, 6,  8,10, 9,  8,11,10,
        12,14,13, 12,15,14, 16,17,18, 16,18,19, 20,22,21, 20,23,22,
    };

    BufferDesc vbDesc;
    vbDesc.sizeInBytes = sizeof(cubeVerts);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.memoryHint = BufferMemoryHint::DeviceOnly;
    IBuffer* cubeVB = device->CreateBuffer(vbDesc, cubeVerts);
    BufferDesc ibDesc;
    ibDesc.sizeInBytes = sizeof(cubeIndices);
    ibDesc.usage = BufferUsage::Index;
    ibDesc.memoryHint = BufferMemoryHint::DeviceOnly;
    IBuffer* cubeIB = device->CreateBuffer(ibDesc, cubeIndices);
    if (!cubeVB || !cubeIB) {
        eyelib::core::Log::Error("DiligentTest: CreateBuffer (cube VB/IB) failed");
        if (cubeVB) device->DestroyBuffer(cubeVB);
        if (cubeIB) device->DestroyBuffer(cubeIB);
        device->DestroyPipeline(cubePipeline);
        device.reset();
        window.Shutdown();
        return 1;
    }

    eyelib::core::Log::Info("DiligentTest: main loop (rotating cube)");
    ClearValue clearColor = {};
    clearColor.color[0] = 0.15f;
    clearColor.color[1] = 0.15f;
    clearColor.color[2] = 0.2f;
    clearColor.color[3] = 1.f;

    auto startTime = std::chrono::steady_clock::now();

    while (!window.ShouldClose()) {
        window.PollEvents();
        window.RefreshFramebufferSize();
        int w = window.Width();
        int h = window.Height();
        if (w <= 0 || h <= 0) continue;

        float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        float angle = t * 0.5f;

        glm::mat4 model = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
        model = glm::rotate(model, angle * 0.7f, glm::vec3(1.f, 0.f, 0.f));
        glm::mat4 view = glm::lookAt(glm::vec3(0.f, 0.f, 4.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
        float aspect = static_cast<float>(w) / static_cast<float>(h);
        glm::mat4 proj = glm::perspective(glm::radians(60.f), aspect, 0.1f, 100.f);
        // HLSL mul(pos, MVP) = row * matrix; we need MVP^T = (P*V*M)^T so that pos*MVP = (P*V*M)*pos
        glm::mat4 mvp = glm::transpose(proj * view * model);
        // Light direction (world space, normalized) - from above-right so cube gets nice shading
        glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, 1.f, 0.5f));

        ICommandList* cmd = device->GetImmediateCommandList();
        if (!cmd) continue;

        cmd->Begin();
        cmd->SetRenderTarget(nullptr);
        cmd->SetViewport(0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        cmd->ClearDefaultFramebuffer(clearColor);

        // 1) Draw fullscreen green quad (test/solid) – if we see green, swap chain and drawing work
        cmd->BindTexture(dummyTex, 0);
        cmd->BindPipeline(solidPipeline);
        cmd->BindVertexBuffer(quadVB, 0, 0);
        cmd->BindIndexBuffer(quadIB, IndexType::UInt16, 0);
        cmd->DrawIndexed(6, 0, 0);

        // 2) Draw rotating cube (test/cube) – MVP, model (for normals), light direction
        cmd->SetUniformMat4("u_MVP", &mvp[0][0]);
        cmd->SetUniformMat4("u_ModelForNormal", &model[0][0]);
        cmd->SetUniformVec3("u_LightDir", &lightDir.x);
        cmd->BindPipeline(cubePipeline);
        cmd->BindVertexBuffer(cubeVB, 0, 0);
        cmd->BindIndexBuffer(cubeIB, IndexType::UInt16, 0);
        cmd->DrawIndexed(36, 0, 0);
        cmd->End();

        device->Present();
    }

    device->DestroyBuffer(quadVB);
    device->DestroyBuffer(quadIB);
    if (rtDummy) device->DestroyRenderTarget(rtDummy);
    device->DestroyBuffer(cubeVB);
    device->DestroyBuffer(cubeIB);
    device->DestroyPipeline(solidPipeline);
    device->DestroyPipeline(cubePipeline);
    device.reset();
    window.Shutdown();
    eyelib::core::Log::Info("DiligentTest: shutdown OK");
    return 0;
}
