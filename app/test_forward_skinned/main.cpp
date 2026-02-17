// Test app: Forward path + skinned model, OpenGL backend only.
// Same flow as the full player including render thread + RenderFrameSync (to repro flicker).
// Hardcoded scene (no JSON), no overlay/physics/scripting.

#include "AssetBinding.h"
#include "PlayerConfig.h"
#include "RenderFrameSync.h"
#include "WorldRender.h"
#include "WorldAnimation.h"

#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/world/All.h"
#include "api/assets/All.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/world/World.h"
#include "eyelib/math/Types.h"

#include <chrono>
#include <cstdio>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

void AttachConsoleIfNeeded() {
#ifdef _WIN32
    if (GetConsoleWindow() == nullptr && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* dummy;
        freopen_s(&dummy, "CONIN$", "r", stdin);
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
    }
#endif
}

std::string GetDefaultAssetRoot() {
    std::string exeDir = eyelib::api::platform::filesystem::GetDirectory(
        eyelib::api::platform::filesystem::GetExecutablePath());
    if (!exeDir.empty() && exeDir.back() != '/' && exeDir.back() != '\\')
        exeDir += "/";
    std::string candidate = exeDir + "assets";
    if (eyelib::api::platform::filesystem::FileExists(candidate))
        return candidate;
    if (eyelib::api::platform::filesystem::FileExists("assets"))
        return "assets";
    return "assets";
}

/** Build minimal scene in-place: camera, directional light, one skinned mesh (Running.fbx). */
void BuildHardcodedScene(eyelib::api::world::World& world) {
    using namespace eyelib::world::components;
    using eyelib::math::Transform;
    using eyelib::math::Quat;
    using eyelib::math::Vec3;

    ::eyelib::world::World* internal = world.GetInternal();
    if (!internal) return;

    auto& state = internal->GetState();
    auto& transformStore = state.GetTransformStore();

    // Camera
    ::eyelib::world::EntityId cameraEntity = world.CreateEntity("MainCamera");
    Transform cameraTransform;
    cameraTransform.position = Vec3(0.f, 2.f, 10.f);
    cameraTransform.rotation = Quat(1.f, 0.f, 0.f, 0.f);
    cameraTransform.scale = Vec3(1.f, 1.f, 1.f);
    transformStore.Add(cameraEntity, cameraTransform);
    Camera cameraComp;
    cameraComp.isPrimary = true;
    cameraComp.isPerspective = true;
    cameraComp.fov = 50.f;
    cameraComp.aspect = 16.f / 9.f;
    cameraComp.nearPlane = 0.1f;
    cameraComp.farPlane = 1000.f;
    world.SetCamera(cameraEntity, cameraComp);

    // Directional light
    ::eyelib::world::EntityId lightEntity = world.CreateEntity("MainLight");
    Transform lightTransform;
    lightTransform.position = Vec3(2.f, 4.f, 2.f);
    lightTransform.rotation = Quat(1.f, 0.f, 0.f, 0.f);
    lightTransform.scale = Vec3(1.f, 1.f, 1.f);
    transformStore.Add(lightEntity, lightTransform);
    Light lightComp;
    lightComp.type = Light::Type::Directional;
    lightComp.color = Vec3(1.f, 1.0f, 1.0f);
    lightComp.intensity = 0.5f;
    lightComp.range = 20.f;
    lightComp.castShadows = true;
    world.SetLight(lightEntity, lightComp);

    // Skinned model (same mesh as test_forward_skinned.json)
    ::eyelib::world::EntityId modelEntity = world.CreateEntity("SkinnedModel");
    Transform modelTransform;
    modelTransform.position = Vec3(0.f, 0.f, 0.f);
    modelTransform.rotation = Quat(1.f, 0.f, 0.f, 0.f);
    modelTransform.scale = Vec3(0.02f, 0.02f, 0.02f);
    transformStore.Add(modelEntity, modelTransform);
    Renderable renderableComp;
    renderableComp.meshAssetId = "models/animation/Running.fbx";
    renderableComp.materialAssetId = "";
    renderableComp.visible = true;
    world.SetRenderable(modelEntity, renderableComp);
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    AttachConsoleIfNeeded();

    const std::string assetRoot = GetDefaultAssetRoot();

    eyelib::api::core::Engine::Config engineConfig;
    engineConfig.enableLogging = true;
    engineConfig.logFile = "TestForwardSkinned.log";
    engineConfig.headless = false;

    if (!eyelib::api::core::Engine::Initialize(engineConfig)) {
        std::fprintf(stderr, "Engine init failed.\n");
        return 4;
    }

    eyelib::api::world::World world;
    BuildHardcodedScene(world);

    eyelib::api::Window window;
    eyelib::api::WindowConfig windowConfig;
    windowConfig.title = "Test: Forward + Skinned (OpenGL)";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.vsync = true;
    windowConfig.useNativeApiOnly = false;  // OpenGL context

    if (!window.Initialize(windowConfig)) {
        eyelib::api::core::Log::Error("Window init failed.");
        eyelib::api::core::Engine::Shutdown();
        return 8;
    }

    openeye::player::WorldRenderBridge renderBridge;
    if (!renderBridge.Initialize(window,
                                 static_cast<uint32_t>(window.Width()),
                                 static_cast<uint32_t>(window.Height()),
                                 eyelib::render::RenderBackend::OpenGL)) {
        eyelib::api::core::Log::Error("Render init failed.");
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 9;
    }

    eyelib::api::AssetRegistry assetRegistry;
    openeye::player::BindAssets(world, { assetRoot }, assetRegistry, true);

    openeye::player::WorldAnimationBridge animationBridge;
    animationBridge.BuildFromWorld(world, assetRegistry);

    openeye::player::RuntimeOptions options;
    options.assetRoots = { assetRoot };
    options.width = window.Width();
    options.height = window.Height();

    openeye::player::RenderFrameSync frameSync;

    // Warmup: one frame on main thread so shaders compile (same as player). Then context moves to render thread.
    renderBridge.BuildScene(world, options, &animationBridge, &assetRegistry,
        frameSync.GetSceneBuffer(0), frameSync.GetFrameAllocator(0));
    renderBridge.RenderSceneOnly(frameSync.GetSceneBuffer(0),
        static_cast<uint32_t>(window.Width()),
        static_cast<uint32_t>(window.Height()));

    frameSync.SetFenceWaiter([&renderBridge](void* f) { renderBridge.WaitAndDestroyFence(f); });

    std::thread renderThread;
    if (openeye::player::kUseRenderThread) {
        renderThread = std::thread([&window, &frameSync, &renderBridge]() {
            window.MakeContextCurrent();
            for (;;) {
                std::optional<openeye::player::RenderFrameSync::AcquiredFrame> frame = frameSync.AcquireFrame();
                if (!frame) {
                    break;
                }
                window.RefreshFramebufferSize();
                const int fbWidth = window.Width();
                const int fbHeight = window.Height();
                if (fbWidth <= 0 || fbHeight <= 0) {
                    frameSync.ReleaseFrame(frame->slot, nullptr);
                    continue;
                }
                window.ApplyVsync();
                const uint32_t w = static_cast<uint32_t>(fbWidth);
                const uint32_t h = static_cast<uint32_t>(fbHeight);
                renderBridge.ApplyResize(w, h);
                const bool rendered = renderBridge.RenderSceneOnly(frameSync.GetSceneBuffer(frame->slot), w, h);
                void* gpuFence = rendered ? renderBridge.InsertFrameFence() : nullptr;
                if (rendered) {
                    renderBridge.Finish();
                    if (gpuFence) {
                        renderBridge.WaitFence(gpuFence);
                    } else {
                        renderBridge.Finish();
                    }
                    renderBridge.Finish();
                    window.SwapBuffers();
                }
                frameSync.ReleaseFrame(frame->slot, gpuFence);
            }
        });
    }

    openeye::player::OverlaySnapshot emptyOverlay;
    auto prevTime = std::chrono::steady_clock::now();
    bool firstFrameDone = false;
    while (true) {
        if (firstFrameDone && window.ShouldClose()) {
            break;
        }
        window.PollEvents();

        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;
        if (delta <= 0.f || delta > 0.5f)
            delta = 1.f / 60.f;

        animationBridge.Update(delta);

        const int slot = frameSync.GetFillSlot();
        if (slot < 0) {
            break;
        }
        renderBridge.BuildScene(world, options, &animationBridge, &assetRegistry,
            frameSync.GetSceneBuffer(slot), frameSync.GetFrameAllocator(slot));
        const int w = window.Width();
        const int h = window.Height();

        if (openeye::player::kUseRenderThread) {
            frameSync.SubmitFrame(slot,
                static_cast<uint32_t>(w > 0 ? w : 1),
                static_cast<uint32_t>(h > 0 ? h : 1),
                emptyOverlay);
        } else {
            // Single-threaded: run render and present inline (same ordering as render thread).
            window.RefreshFramebufferSize();
            const int fbWidth = window.Width();
            const int fbHeight = window.Height();
            if (fbWidth > 0 && fbHeight > 0) {
                window.ApplyVsync();
                const uint32_t rw = static_cast<uint32_t>(fbWidth);
                const uint32_t rh = static_cast<uint32_t>(fbHeight);
                renderBridge.ApplyResize(rw, rh);
                const bool rendered = renderBridge.RenderSceneOnly(frameSync.GetSceneBuffer(slot), rw, rh);
                void* gpuFence = rendered ? renderBridge.InsertFrameFence() : nullptr;
                if (rendered) {
                    renderBridge.Finish();
                    if (gpuFence) {
                        renderBridge.WaitFence(gpuFence);
                    } else {
                        renderBridge.Finish();
                    }
                    renderBridge.Finish();
                    window.SwapBuffers();
                }
                frameSync.ReleaseFrame(slot, gpuFence);
            } else {
                frameSync.ReleaseFrame(slot, nullptr);
            }
        }
        firstFrameDone = true;
    }

    if (openeye::player::kUseRenderThread) {
        frameSync.SignalShutdown();
        renderThread.join();
    }

    window.MakeContextCurrent();
    renderBridge.Shutdown();
    window.Shutdown();
    eyelib::api::core::Engine::Shutdown();
    return 0;
}
