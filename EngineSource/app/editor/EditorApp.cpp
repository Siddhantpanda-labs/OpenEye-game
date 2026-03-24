#include "EditorApp.h"

#include "EditorContext.h"
#include "EditorProject.h"
#include "EditorSceneBuilder.h"
#include "ScenePipelineMode.h"
#include "MenuBar.h"
#include "panels/StatusPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"
#include "panels/GameViewPanel.h"
#include "panels/AssetPanel.h"
#include "panels/ProfilerPanel.h"
#include "GameRuntime.h"

#include "api/asset/All.h"
#include "api/asset/GPUResourceResolverBackend.h"
#include "api/core/All.h"
#include "api/physics/PhysicsModule.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "api/render/RenderDevice.h"
#include "api/platform/GraphicsDevice.h"
#include "api/render/RenderBackend.h"
#include "api/render/StaticResources.h"
#include "api/debug/All.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#ifdef OPENEYE_IMGUI_VULKAN
#include <vulkan/vulkan.h>
#include <backends/imgui_impl_vulkan.h>
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#endif
#include "eyelib/render/rhi/RHI.h"
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace openeye::editor {

namespace {

struct ApiAssetIdHash {
    size_t operator()(const eyelib::api::AssetId& id) const {
        return id.high ^ (id.low << 1);
    }
};

class EditorGPUResourceCache : public eyelib::api::IGPUResourceResolver {
public:
    EditorGPUResourceCache(void* device, eyelib::api::AssetRegistry* registry)
        : m_device(device), m_registry(registry) {}

    std::shared_ptr<eyelib::render::Shader> GetShader(const eyelib::api::AssetId& id) override {
        if (!id.IsValid()) return nullptr;
        auto it = m_shaders.find(id);
        if (it != m_shaders.end()) return it->second;
        const auto* desc = m_registry ? m_registry->GetShaderDescriptor(id) : nullptr;
        if (!desc) return nullptr;
        auto sh = std::make_shared<eyelib::render::Shader>();
        if (!sh->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
            return nullptr;
        m_shaders[id] = sh;
        return sh;
    }

    std::shared_ptr<eyelib::render::Mesh> GetMesh(const eyelib::api::AssetId& id) override {
        if (!id.IsValid()) return nullptr;
        auto it = m_meshes.find(id);
        if (it != m_meshes.end()) return it->second;
        const auto* desc = m_registry ? m_registry->GetMeshDescriptor(id) : nullptr;
        if (!desc) return nullptr;
        auto mesh = std::make_shared<eyelib::render::Mesh>();
        if (!mesh->CreateFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
            return nullptr;
        m_meshes[id] = mesh;
        return mesh;
    }

    std::shared_ptr<eyelib::render::Texture2D> GetTexture(const eyelib::api::AssetId& id) override {
        if (!id.IsValid()) return nullptr;
        auto it = m_textures.find(id);
        if (it != m_textures.end()) return it->second;
        const auto* desc = m_registry ? m_registry->GetTextureDescriptor(id) : nullptr;
        if (!desc || desc->filePath.empty()) return nullptr;
        auto tex = std::make_shared<eyelib::render::Texture2D>();
        if (!tex->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
            return nullptr;
        m_textures[id] = tex;
        return tex;
    }

    std::shared_ptr<eyelib::render::Material> GetMaterial(const eyelib::api::AssetId& id) override {
        if (!id.IsValid()) return nullptr;
        auto it = m_materials.find(id);
        if (it != m_materials.end()) return it->second;
        const auto* desc = m_registry ? m_registry->GetMaterialDescriptor(id) : nullptr;
        if (!desc || !m_registry) return nullptr;
        auto resolveShader = [this](const std::string& name) {
            eyelib::api::AssetId sid = m_registry->GetId("name:" + name);
            if (!sid.IsValid()) sid = m_registry->LoadAssetDescriptorOnly("name:" + name, eyelib::api::AssetType::Shader);
            return GetShader(sid);
        };
        auto resolveTexture = [this](const std::string& path) {
            eyelib::api::AssetId tid = m_registry->GetId(path);
            if (!tid.IsValid()) tid = m_registry->LoadAssetDescriptorOnly(path, eyelib::api::AssetType::Texture);
            return GetTexture(tid);
        };
        auto mat = eyelib::render::CreateMaterialFromDescriptor(*desc, resolveShader, resolveTexture);
        if (!mat) return nullptr;
        m_materials[id] = mat;
        return mat;
    }

    void OnDeviceLost() override {
        m_shaders.clear();
        m_meshes.clear();
        m_textures.clear();
        m_materials.clear();
    }

private:
    void* m_device = nullptr;
    eyelib::api::AssetRegistry* m_registry = nullptr;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Shader>, ApiAssetIdHash> m_shaders;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Mesh>, ApiAssetIdHash> m_meshes;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Texture2D>, ApiAssetIdHash> m_textures;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Material>, ApiAssetIdHash> m_materials;
};

class ScopedEngine {
public:
    bool Initialize(const eyelib::api::Engine::Config& config) {
        m_initialized = eyelib::api::Engine::Initialize(config);
        return m_initialized;
    }
    ~ScopedEngine() {
        if (m_initialized) {
            eyelib::api::Engine::Shutdown();
        }
    }
private:
    bool m_initialized = false;
};

class ScopedWindow {
public:
    bool Initialize(eyelib::api::Window& window, const eyelib::api::WindowConfig& config) {
        if (!window.Initialize(config)) {
            return false;
        }
        m_window = &window;
        return true;
    }
    ~ScopedWindow() {
        if (m_window) {
            m_window->Shutdown();
        }
    }
private:
    eyelib::api::Window* m_window = nullptr;
};

class ScopedImGui {
public:
    bool Initialize(eyelib::api::Window& window, void* device) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        auto* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeHandle());
#ifdef OPENEYE_IMGUI_VULKAN
        if (device) {
            auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(device);
            eyelib::render::rhi::ImGuiVulkanInitInfo info = {};
            if (rhiDevice->GetImGuiVulkanInitInfo(&info)) {
                ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);
                ImGui_ImplVulkan_InitInfo vkInfo = {};
                vkInfo.ApiVersion = VK_API_VERSION_1_0;
                vkInfo.Instance = static_cast<VkInstance>(info.instance);
                vkInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(info.physical_device);
                vkInfo.Device = static_cast<VkDevice>(info.device);
                vkInfo.QueueFamily = info.queue_family;
                vkInfo.Queue = static_cast<VkQueue>(info.queue);
                vkInfo.DescriptorPool = static_cast<VkDescriptorPool>(info.descriptor_pool);
                vkInfo.RenderPass = static_cast<VkRenderPass>(info.render_pass);
                vkInfo.MinImageCount = info.min_image_count;
                vkInfo.ImageCount = info.image_count;
                vkInfo.PipelineCache = static_cast<VkPipelineCache>(info.pipeline_cache);
                if (ImGui_ImplVulkan_Init(&vkInfo)) {
                    auto* vkDeviceObj = static_cast<eyelib::render::rhi::DeviceVK*>(rhiDevice);
                    m_vulkanSampler = static_cast<VkSampler>(vkDeviceObj->GetDefaultSampler());
                    m_vkDevice = static_cast<VkDevice>(info.device);
                    if (!CreateViewportDescriptorSets()) {
                        ImGui_ImplVulkan_Shutdown();
                        ImGui_ImplGlfw_Shutdown();
                        ImGui::DestroyContext();
                        return false;
                    }
                    rhiDevice->SetPostBlitCallback([](void* cmd) {
                        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));
                    });
                    m_vulkan = true;
                    m_ready = true;
                    return true;
                }
                ImGui_ImplGlfw_Shutdown();
            }
            ImGui::DestroyContext();
            return false;
        }
        ImGui::DestroyContext();
        return false;
#else
        (void)glfwWindow;
        (void)device;
        ImGui::DestroyContext();
        return false;
#endif
    }
    ~ScopedImGui() {
        if (!m_ready) return;
        if (m_vulkan) {
#ifdef OPENEYE_IMGUI_VULKAN
            DestroyViewportDescriptorSets();
            ImGui_ImplVulkan_Shutdown();
#endif
        } else {
            ImGui_ImplOpenGL3_Shutdown();
        }
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    bool IsVulkan() const { return m_vulkan; }
    /** Call at start of each frame (after device BeginFrame). Stores frame index and resets slot so we never update a set still in use by a pending command buffer. */
    void PruneTextureDescriptors(eyelib::render::rhi::IDevice* rhiDevice) {
#ifdef OPENEYE_IMGUI_VULKAN
        if (!m_vulkan) return;
        if (rhiDevice) m_currentFrameIndex = rhiDevice->GetCurrentFrameIndex() % kViewportFramesInFlight;
        m_resolveCallIndex = 0;
#endif
    }
    /** Returns an ImGui texture ID (VkDescriptorSet) for the given pipeline output image view. Uses per-frame sets (frameIndex * kViewportSlots + slot) so we never update a set in use. */
    void* ResolveTextureId(void* nativeTexture) {
#ifdef OPENEYE_IMGUI_VULKAN
        if (!m_vulkan) return nativeTexture;
        if (!nativeTexture || m_vulkanSampler == VK_NULL_HANDLE || m_vkDevice == VK_NULL_HANDLE) return nullptr;
        const uint32_t slot = (m_resolveCallIndex < kViewportSlots) ? m_resolveCallIndex : (kViewportSlots - 1u);
        m_resolveCallIndex++;
        const uint32_t setIndex = m_currentFrameIndex * kViewportSlots + slot;
        VkDescriptorSet set = m_viewportSets[setIndex];
        if (set == VK_NULL_HANDLE) return nullptr;
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler = m_vulkanSampler;
        imageInfo.imageView = static_cast<VkImageView>(nativeTexture);
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_vkDevice, 1, &write, 0, nullptr);
        return reinterpret_cast<void*>(set);
#else
        return nativeTexture;
#endif
    }
private:
    bool m_ready = false;
    bool m_vulkan = false;
#ifdef OPENEYE_IMGUI_VULKAN
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkSampler m_vulkanSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_viewportSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_viewportDescriptorPool = VK_NULL_HANDLE;
    static constexpr uint32_t kViewportFramesInFlight = 3;
    static constexpr uint32_t kViewportSlots = 2;
    static constexpr uint32_t kViewportSetCount = kViewportFramesInFlight * kViewportSlots;
    VkDescriptorSet m_viewportSets[kViewportSetCount] = {};
    uint32_t m_resolveCallIndex = 0;
    uint32_t m_currentFrameIndex = 0;

    bool CreateViewportDescriptorSets() {
        if (m_vkDevice == VK_NULL_HANDLE) return false;
        for (uint32_t i = 0; i < kViewportSetCount; ++i) m_viewportSets[i] = VK_NULL_HANDLE;
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr, &m_viewportSetLayout) != VK_SUCCESS)
            return false;
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = kViewportSetCount;
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = kViewportSetCount;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(m_vkDevice, &poolInfo, nullptr, &m_viewportDescriptorPool) != VK_SUCCESS) {
            vkDestroyDescriptorSetLayout(m_vkDevice, m_viewportSetLayout, nullptr);
            m_viewportSetLayout = VK_NULL_HANDLE;
            return false;
        }
        VkDescriptorSetLayout layouts[kViewportSetCount];
        for (uint32_t i = 0; i < kViewportSetCount; ++i) layouts[i] = m_viewportSetLayout;
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_viewportDescriptorPool;
        allocInfo.descriptorSetCount = kViewportSetCount;
        allocInfo.pSetLayouts = layouts;
        if (vkAllocateDescriptorSets(m_vkDevice, &allocInfo, m_viewportSets) != VK_SUCCESS) {
            vkDestroyDescriptorPool(m_vkDevice, m_viewportDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(m_vkDevice, m_viewportSetLayout, nullptr);
            m_viewportDescriptorPool = VK_NULL_HANDLE;
            m_viewportSetLayout = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }
    void DestroyViewportDescriptorSets() {
        if (m_vkDevice == VK_NULL_HANDLE) return;
        if (m_viewportDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_vkDevice, m_viewportDescriptorPool, nullptr);
            m_viewportDescriptorPool = VK_NULL_HANDLE;
            for (uint32_t i = 0; i < kViewportSetCount; ++i) m_viewportSets[i] = VK_NULL_HANDLE;
        }
        if (m_viewportSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_vkDevice, m_viewportSetLayout, nullptr);
            m_viewportSetLayout = VK_NULL_HANDLE;
        }
    }
#endif
};

} // namespace

int RunEditor(int argc, char** argv) {
    const char* projectPathArg = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
            projectPathArg = argv[i + 1];
            break;
        }
    }

    eyelib::api::Engine::Config engineConfig;
    engineConfig.enableLogging = true;
    engineConfig.logFile = "editor.log";
    engineConfig.headless = false;

    eyelib::api::Engine::RegisterSubsystem(std::make_unique<eyelib::api::PhysicsModule>());
    ScopedEngine engine;
    if (!engine.Initialize(engineConfig)) {
        std::fprintf(stderr, "Editor: Engine init failed.\n");
        return 1;
    }

    eyelib::api::Window window;
    eyelib::api::WindowConfig windowConfig;
    windowConfig.title = "OpenEye Editor";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.vsync = true;
    windowConfig.resizable = true;
    windowConfig.useNativeApiOnly = true;

    ScopedWindow windowGuard;
    if (!windowGuard.Initialize(window, windowConfig)) {
        std::fprintf(stderr, "Editor: Window init failed.\n");
        return 2;
    }

    // Resolve engine root and set working directory before RHI/pipeline init so shader paths
    // (including .spv) resolve correctly during warmup.
    std::string engineRoot = eyelib::api::platform::filesystem::GetEngineAssetsRoot();
    std::string engineAssets = "assets";
    if (!engineRoot.empty()) {
        engineAssets = eyelib::api::platform::filesystem::NormalizePath(engineRoot + "/assets");
        eyelib::api::platform::filesystem::SetWorkingDirectory(engineRoot);
    }

    // One RHI device per window; pipelines are view configs only (no pipeline-owned device).
    // Prefer Vulkan; fall back to OpenGL if Vulkan init fails so the editor always starts.
    void* platformWindow = window.GetPlatformWindow();
    const uint32_t deviceInitWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Width()));
    const uint32_t deviceInitHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Height()));
    auto editorDevice = platformWindow
        ? eyelib::platform::CreateGraphicsDevice(
              static_cast<eyelib::platform::Window*>(platformWindow), deviceInitWidth, deviceInitHeight,
              eyelib::render::RenderBackend::Vulkan)
        : nullptr;
    if (!editorDevice) {
        std::fprintf(stderr, "Editor: Vulkan device init failed.\n");
        return 3;
    }
    eyelib::api::RenderDevice editorRenderDevice(editorDevice.get());

    ScopedImGui imgui;
    if (!imgui.Initialize(window, editorDevice.get())) {
        std::fprintf(stderr, "Editor: ImGui init failed.\n");
        return 3;
    }

    eyelib::api::Renderer sceneViewRenderer;
    eyelib::api::RenderPipeline sceneViewPipeline;
    eyelib::api::RenderPipeline gameViewPipeline;
    if (!sceneViewRenderer.Initialize()) {
        std::fprintf(stderr, "Editor: Renderer init failed.\n");
    }
    if (!editorDevice) {
        std::fprintf(stderr, "Editor: CreateGraphicsDevice failed (scene/game view will show placeholder).\n");
    } else {
        bool scenePipelineOk = false;
        if (!(scenePipelineOk = sceneViewPipeline.InitializeWithSharedDevice(editorRenderDevice, 256, 256))) {
            std::fprintf(stderr, "Editor: Scene view pipeline init failed (scene view will show placeholder).\n");
        }
        bool gamePipelineOk = scenePipelineOk
            ? gameViewPipeline.InitializeWithSharedDevice(editorRenderDevice, 256, 256, &sceneViewPipeline)
            : gameViewPipeline.InitializeWithSharedDevice(editorRenderDevice, 256, 256);
        if (!gamePipelineOk) {
            std::fprintf(stderr, "Editor: Game view pipeline init failed (debug camera preview disabled).\n");
        }
        eyelib::api::core::Log::Info("Editor: both pipelines initialized, before AssetRegistry.Scan");
    }

    eyelib::api::AssetRegistry assetRegistry;
    assetRegistry.SetAssetRoot(engineAssets);
    assetRegistry.Scan();
    eyelib::api::core::Log::Info("Editor: AssetRegistry.Scan done");
    if (editorDevice)
        assetRegistry.SetRenderDevice(editorRenderDevice);
    std::unique_ptr<EditorGPUResourceCache> editorGpuResourceCache;
    if (editorDevice) {
        editorGpuResourceCache = std::make_unique<EditorGPUResourceCache>(editorRenderDevice.Get(), &assetRegistry);
        assetRegistry.SetGPUResolver(editorGpuResourceCache.get());
    }

    openeye::editor::EditorContext ctx;
    ctx.sceneViewPipeline = &sceneViewPipeline;
    ctx.gameViewPipeline = &gameViewPipeline;
    ctx.assetRegistry = &assetRegistry;
    ctx.assetRoots = { engineAssets };
    ctx.engineAssetsPath = engineAssets;
    ctx.window = &window;
    ctx.resolveImGuiTextureId = [&imgui](void* nativeTexture) -> void* {
        return imgui.ResolveTextureId(nativeTexture);
    };
    openeye::editor::ApplyScenePipelineModeToEditorPipelines(ctx);
    eyelib::api::core::Log::Info("Editor: ApplyScenePipelineModeToEditorPipelines done");

    if (projectPathArg && openeye::editor::OpenProject(ctx, projectPathArg)) {
        eyelib::api::core::Log::InfoF("Editor: opened project '%s' at %s", ctx.projectName.c_str(), ctx.projectPath.c_str());
    }

    if (editorRenderDevice) {
        auto defaultMat = std::make_shared<eyelib::api::Material>();
        defaultMat->SetMaterialShaderId(0);  // PBR; variant from ShaderCache at draw time
        ctx.defaultMaterial = std::move(defaultMat);
    }

    uint32_t lastFramebufferWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Width()));
    uint32_t lastFramebufferHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Height()));
    eyelib::api::core::Log::Info("Editor: init complete, entering main loop");
    while (!window.ShouldClose()) {
        window.PollEvents();
        window.MakeContextCurrent();
        window.RefreshFramebufferSize();
        const uint32_t framebufferWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Width()));
        const uint32_t framebufferHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(window.Height()));
        if (editorDevice && (framebufferWidth != lastFramebufferWidth || framebufferHeight != lastFramebufferHeight)) {
            auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(editorDevice.get());
            rhiDevice->OnWindowResized(framebufferWidth, framebufferHeight);
            lastFramebufferWidth = framebufferWidth;
            lastFramebufferHeight = framebufferHeight;
        }
        if (ctx.gameRuntimeStopRequested && ctx.gameRuntime) {
            ctx.world.SetFrameSnapshot(nullptr);
            ctx.world.SetCaptureSnapshotCallback(nullptr);
            try {
                ctx.gameRuntime.reset();
            } catch (...) {}
            ctx.world.SetFrameSnapshot(nullptr);
            ctx.gameRuntimeStopRequested = false;
            // Model A: single device; no restore needed.
            // Revert world to pre-Play snapshot so edits during Play are discarded.
            if (!ctx.playSnapshotPath.empty()) {
                if (ctx.world.LoadScene(ctx.playSnapshotPath)) {
                    if (ctx.assetRegistry) {
                        EditorSceneBuilder::BindSceneAssets(ctx.world, ctx.assetRoots, *ctx.assetRegistry);
                    }
                    openeye::editor::ApplyScenePipelineModeToEditorPipelines(ctx);
                    ctx.selectedEntity = ::eyelib::world::InvalidEntity;
                    ctx.sceneSelected = false;
                }
                (void)eyelib::api::platform::filesystem::DeleteFile(ctx.playSnapshotPath);
                ctx.playSnapshotPath.clear();
            }
            eyelib::api::core::Log::Info("Game runtime stopped; default device restored.");
        }
        window.Clear(0.2f, 0.2f, 0.25f, 1.0f);

        eyelib::api::debug::Profiler::BeginFrame();

        if (imgui.IsVulkan() && editorDevice) {
#ifdef OPENEYE_IMGUI_VULKAN
            auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(editorDevice.get());
            rhiDevice->BeginFrame();
            imgui.PruneTextureDescriptors(rhiDevice);
            ImGui_ImplVulkan_NewFrame();
#endif
        } else {
            ImGui_ImplOpenGL3_NewFrame();
        }
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        if (ctx.sceneViewPipeline)
            ctx.sceneViewPipeline->UpdateUploads();

        openeye::editor::ShowMenuBar(ctx);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        openeye::editor::panels::ShowStatusPanel(&ctx);
        openeye::editor::panels::ShowHierarchyPanel(&ctx);
        openeye::editor::panels::ShowInspectorPanel(&ctx);
        openeye::editor::panels::ShowAssetPanel(&ctx);
        openeye::editor::panels::ShowSceneViewPanel(&ctx);
        openeye::editor::panels::ShowGameViewPanel(&ctx);
        openeye::editor::panels::ShowProfilerPanel(&ctx);

        ImGui::Render();
        if (imgui.IsVulkan() && editorDevice) {
            auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(editorDevice.get());
            rhiDevice->BlitToDefaultFramebuffer(nullptr);
            rhiDevice->EndFrame();
            rhiDevice->Present();
            // One-time readback: after frame 30, wait for GPU then read 1 pixel from scene pipeline output.
            {
                static int s_frameCount = 0;
                if (ctx.sceneViewPipeline && ++s_frameCount == 30) {
                    ctx.sceneViewPipeline->Finish();
                    uint8_t px[4] = {};
                    if (ctx.sceneViewPipeline->ReadOutputPixels(1, 1, px)) {
                        eyelib::api::core::Log::InfoF("Scene output readback (1x1): R=%u G=%u B=%u A=%u (non-zero => draw wrote)", px[0], px[1], px[2], px[3]);
                    } else {
                        eyelib::api::core::Log::Info("Scene output readback: ReadOutputPixels failed");
                    }
                }
            }
        } else {
            if (ctx.sceneViewPipeline)
                ctx.sceneViewPipeline->RestoreDefaultFramebufferAndViewport(
                    static_cast<uint32_t>(window.Width()), static_cast<uint32_t>(window.Height()));
            if (ctx.gameViewPipeline)
                ctx.gameViewPipeline->RestoreDefaultFramebufferAndViewport(
                    static_cast<uint32_t>(window.Width()), static_cast<uint32_t>(window.Height()));
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            window.SwapBuffers();
        }

        eyelib::api::debug::Profiler::EndFrame();
    }

    // Teardown: wait for GPU so no resources are in use, then pipelines release FBOs; device destroyed when editorDevice goes out of scope.
    if (imgui.IsVulkan() && editorDevice) {
        static_cast<eyelib::render::rhi::IDevice*>(editorDevice.get())->Finish();
    }
    sceneViewPipeline.Shutdown();
    gameViewPipeline.Shutdown();
    assetRegistry.SetGPUResolver(nullptr);
    if (editorDevice) {
        eyelib::render::DebugDrawRenderer::Shutdown();
        eyelib::render::Primitives::ShutdownStaticResources();
    }

    return 0;
}

} // namespace openeye::editor
