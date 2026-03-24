/**
 * Vulkan test app: load an image (assets/textures/checke.png) and display it fullscreen
 * using the RHI Vulkan backend and the existing passes/post/copy shader.
 *
 * Run from project root or Engine/bin so assets are found. Build with OPENEYE_BUILD_TESTS=ON.
 *
 * kOffscreenSolidColorDebug: true = solid pink offscreen RT only (tests swapchain blit). false = show checke.png.
 */

#include "api/core/All.h"
#include "api/platform/Window.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/platform/window/Window.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace eyelib::render;
namespace rhi = eyelib::render::rhi;

namespace {

constexpr uint32_t kWidth = 1280u;
constexpr uint32_t kHeight = 720u;

/** If true: offscreen RT is only cleared to kOffscreenClear (no texture draw) — verifies blit + present. Set false for checke.png. */
constexpr bool kOffscreenSolidColorDebug = false;
// Linear clear RGBA; obvious on screen (pink) when blit path works.
constexpr rhi::ClearValue kOffscreenClear = { 1.0f, 0.25f, 0.0f, 1.0f };

std::string ResolveImagePath() {
    const std::string candidates[] = {
        "../../assets/textures/checke.png",
        "EngineSource/assets/textures/checke.png",
        "../assets/textures/checke.png",
    };
    std::string root = eyelib::platform::filesystem::GetEngineAssetsRoot();
    if (!root.empty()) {
        std::string fromRoot = eyelib::platform::filesystem::NormalizePath(root + "/textures/checke.png");
        if (eyelib::platform::filesystem::FileExists(fromRoot))
            return fromRoot;
    }
    for (const auto& p : candidates) {
        if (eyelib::platform::filesystem::FileExists(p))
            return p;
    }
    return candidates[0]; // return default for error message
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
    winConfig.title = "Vulkan Test (fullscreen image)";
    winConfig.width = static_cast<int>(kWidth);
    winConfig.height = static_cast<int>(kHeight);
    winConfig.useNativeApiOnly = true; // no OpenGL context for Vulkan
    if (!window.Initialize(winConfig)) {
        std::cerr << "VulkanTest: Window init failed\n";
        eyelib::api::core::Engine::Shutdown();
        return 3;
    }

    eyelib::platform::Window* platformWindow = static_cast<eyelib::platform::Window*>(window.GetPlatformWindow());
    if (!platformWindow) {
        std::cerr << "VulkanTest: GetPlatformWindow failed\n";
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 4;
    }

    std::unique_ptr<rhi::IDevice> deviceHolder =
        eyelib::platform::CreateGraphicsDevice(platformWindow, kWidth, kHeight, eyelib::render::RenderBackend::Vulkan);
    if (!deviceHolder) {
        std::cerr << "VulkanTest: CreateGraphicsDevice (Vulkan) failed\n";
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 5;
    }
    eyelib::render::rhi::IDevice* device = deviceHolder.get();

    // Load image texture
    std::string imagePath = ResolveImagePath();
    auto texture2D = std::make_shared<eyelib::render::Texture2D>();
    texture2D->SetDevice(device);
    if (!texture2D->LoadFromFile(device, imagePath, true, false)) {
        std::cerr << "VulkanTest: failed to load image: " << imagePath << " (run from project root or set cwd)\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 6;
    }
    rhi::ITexture* imageTexture = texture2D->GetTextureRHI();
    if (!imageTexture) {
        std::cerr << "VulkanTest: texture RHI is null\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 7;
    }

    // Copy shader (fullscreen quad, samples u_Texture at set=1 binding=0)
    auto copyShader = std::make_shared<eyelib::render::Shader>();
    rhi::VertexLayout fullscreenLayout = eyelib::render::Mesh::GetFullscreenQuadVertexLayoutRHI();
    rhi::DepthStencilState noDepth;
    noDepth.depthTest = false;
    noDepth.depthWrite = false;
    if (!copyShader->Load(device, "passes/post/copy", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, &noDepth, rhi::PipelineLayoutKind::Post,
                          std::optional{rhi::StandardAttachmentFormats::PostCopyBGRA()})) {
        std::cerr << "VulkanTest: failed to load passes/post/copy shader (run from project root so assets/shaders/ are found)\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 8;
    }
    rhi::IPipeline* copyPipeline = copyShader->GetPipelineRHI();
    if (!copyPipeline) {
        std::cerr << "VulkanTest: copy shader pipeline is null\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 9;
    }

    // Offscreen render target (same size as window) to draw the image into, then blit to swapchain
    // Must match swapchain format (B8G8R8A8_UNORM on Windows) so vkCmdCopyImage in BlitToDefaultFramebuffer is valid.
    std::vector<rhi::RHITextureFormat> colorFormats = { rhi::RHITextureFormat::BGRA8 };
    rhi::IRenderTarget* offscreenRT = device->CreateRenderTarget(kWidth, kHeight, colorFormats, rhi::RHITextureFormat::None, 1, false);
    if (!offscreenRT) {
        std::cerr << "VulkanTest: CreateRenderTarget failed\n";
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 10;
    }

    // Descriptor set for PostPass (set 1): single texture at binding 0
    rhi::IPipelineLayout* postLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
    if (!postLayout) {
        std::cerr << "VulkanTest: PostPass layout is null\n";
        device->DestroyRenderTarget(offscreenRT);
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 11;
    }
    rhi::IDescriptorSet* postSet = device->CreateDescriptorSet(postLayout);
    if (!postSet) {
        std::cerr << "VulkanTest: CreateDescriptorSet(PostPass) failed\n";
        device->DestroyRenderTarget(offscreenRT);
        deviceHolder.reset();
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 12;
    }
    postSet->UpdateTexture(0, imageTexture);

    // Frame set (set 0) required by Post pipeline layout: minimal dummy UBOs
    rhi::IPipelineLayout* frameLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
    rhi::IDescriptorSet* frameSet = nullptr;
    rhi::IBuffer* frameUBO0 = nullptr;
    rhi::IBuffer* frameUBO1 = nullptr;
    if (frameLayout) {
        frameSet = device->CreateDescriptorSet(frameLayout);
        if (frameSet) {
            rhi::BufferDesc ubDesc;
            ubDesc.sizeInBytes = 256;
            ubDesc.usage = rhi::BufferUsage::Uniform;
            ubDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
            frameUBO0 = device->CreateBuffer(ubDesc, nullptr);
            frameUBO1 = device->CreateBuffer(ubDesc, nullptr);
            if (frameUBO0 && frameUBO1) {
                frameSet->UpdateUniformBuffer(0, frameUBO0, 0, 256);
                frameSet->UpdateUniformBuffer(1, frameUBO1, 0, 256);
            }
        }
    }

    std::cout << "VulkanTest: running. Close window to exit.\n";

    while (!window.ShouldClose()) {
        window.PollEvents();
        window.MakeContextCurrent();

        uint32_t fbW = static_cast<uint32_t>(std::max(1, window.Width()));
        uint32_t fbH = static_cast<uint32_t>(std::max(1, window.Height()));
        if (fbW != kWidth || fbH != kHeight)
            device->OnWindowResized(fbW, fbH);

        device->BeginFrame();
        // if constexpr (kOffscreenSolidColorDebug) {
        //     (void)copyShader;
        //     (void)copyPipeline;
        //     (void)imageTexture;
        //     (void)postSet;
        //     (void)frameSet;
        // }
        rhi::ICommandList* cmd = device->GetImmediateCommandList();
        if (!cmd) {
            device->EndFrame();
            device->Present();
            continue;
        }

        // if (!kOffscreenSolidColorDebug) {
            device->TransitionTextureToState(imageTexture, rhi::ResourceState::ShaderResource);
            device->FlushBarrierTracker(cmd);
        // }

        rhi::ClearAndSetRenderTargetWithViewport(cmd, offscreenRT, 0, 0, kWidth, kHeight, { 0.12f, 0.12f, 0.15f, 1.0f });
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(true);

        // if (!kOffscreenSolidColorDebug) {
            copyShader->Bind(device);
            copyShader->SetInt("u_Texture", 0);
            if (frameSet && postSet) {
                rhi::IDescriptorSet* sets[2] = { frameSet, postSet };
                cmd->BindDescriptorSets(0, 2, sets);
            } else {
                cmd->BindDescriptorSets(1, 1, &postSet);
            }
            cmd->DrawFullscreenQuad(copyPipeline);
            copyShader->Unbind();
        // }

        rhi::ITexture* rtColor = offscreenRT->GetColorTexture(0);
        if (rtColor)
            rhi::BarrierTextureForShaderRead(cmd, rtColor);

        device->BlitToDefaultFramebuffer(offscreenRT);
        device->EndFrame();
        device->Present();
    }

    if (frameUBO0) device->DestroyBuffer(frameUBO0);
    if (frameUBO1) device->DestroyBuffer(frameUBO1);
    if (frameSet) device->DestroyDescriptorSet(frameSet);
    device->DestroyDescriptorSet(postSet);
    device->DestroyRenderTarget(offscreenRT);
    deviceHolder.reset();
    window.Shutdown();
    eyelib::api::core::Engine::Shutdown();
    return 0;
}
