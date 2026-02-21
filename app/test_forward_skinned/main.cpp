// Test app: skybox + camera orbit only.
// Renders the Larnaca Beach skybox; right-drag to rotate the camera.
// Single-threaded, no scene geometry.

#include "AssetBinding.h"
#include "PlayerConfig.h"
#include "RenderFrameSync.h"
#include "WorldRender.h"
#include "WorldAnimation.h"

#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/TextureCube.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"
#include "api/math/TransformUtils.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/math/Types.h"

#include <cmath>
#include <cstdio>

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

std::string GetAssetRoot() {
    std::string exeDir = eyelib::api::platform::filesystem::GetDirectory(
        eyelib::api::platform::filesystem::GetExecutablePath());
    if (!exeDir.empty() && exeDir.back() != '/' && exeDir.back() != '\\')
        exeDir += "/";
    std::string cand = exeDir + "assets";
    if (eyelib::api::platform::filesystem::FileExists(cand)) return cand;
    return eyelib::api::platform::filesystem::FileExists("assets") ? "assets" : "assets";
}

std::string ResolvePath(const std::string& root, const std::string& id) {
    if (eyelib::api::platform::filesystem::FileExists(id)) return id;
    std::string p = root.empty() ? id : (root.back() == '/' || root.back() == '\\' ? root + id : root + "/" + id);
    return eyelib::api::platform::filesystem::FileExists(p) ? p : "";
}

/** Load Skybox/LarnacaBeach. Call after render init (GL context current). Returns null if failed. */
std::shared_ptr<eyelib::api::TextureCube> LoadSkybox(const std::string& assetRoot) {
    const char* faces[] = { "posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg" };
    std::string base = "Skybox/LarnacaBeach";
    std::string paths[6];
    for (int i = 0; i < 6; ++i) {
        paths[i] = ResolvePath(assetRoot, base + "/" + faces[i]);
        if (paths[i].empty()) {
            eyelib::api::core::Log::WarnF("Skybox face missing: %s/%s", base.c_str(), faces[i]);
            return nullptr;
        }
    }
    auto tex = std::make_shared<eyelib::api::TextureCube>();
    if (!tex->LoadFromFiles(paths[0], paths[1], paths[2], paths[3], paths[4], paths[5], true, true)) {
        eyelib::api::core::Log::WarnF("Skybox LoadFromFiles failed");
        return nullptr;
    }
    eyelib::api::core::Log::Info("Skybox loaded.");
    return tex;
}

struct OrbitState {
    float yaw = 0.f;
    float pitch = 0.3f;
    float distance = 10.f;
    eyelib::math::Vec3 target{0.f, 2.f, 0.f};
    static constexpr float sensitivity = 0.004f;
    static constexpr float pitchMin = -1.4f;
    static constexpr float pitchMax = 1.4f;
};

void UpdateOrbit(eyelib::api::world::World& world, ::eyelib::world::EntityId cameraEntity, OrbitState& orbit) {
    using namespace eyelib::world::components;
    using eyelib::math::Vec3;
    using eyelib::math::Quat;
    using eyelib::math::Mat4;

    if (eyelib::api::Input::IsMouseButtonPressed(1)) {
        auto d = eyelib::api::Input::MouseDelta();
        orbit.yaw += d.x * orbit.sensitivity;
        orbit.pitch -= d.y * orbit.sensitivity;
        if (orbit.pitch > orbit.pitchMax) orbit.pitch = orbit.pitchMax;
        if (orbit.pitch < orbit.pitchMin) orbit.pitch = orbit.pitchMin;
    }

    float cy = std::cos(orbit.pitch), sy = std::sin(orbit.pitch);
    float ct = std::cos(orbit.yaw), st = std::sin(orbit.yaw);
    Vec3 forward(cy * st, sy, -cy * ct);
    Vec3 position = orbit.target - orbit.distance * forward;

    Mat4 view = eyelib::math::LookAt(position, orbit.target, Vec3(0.f, 1.f, 0.f));
    Quat rotation = glm::quat_cast(glm::mat3(glm::inverse(view)));

    Transform tr;
    if (!world.GetTransform(cameraEntity, tr)) return;
    tr.position = position;
    tr.rotation = rotation;
    tr.scale = Vec3(1.f, 1.f, 1.f);
    world.SetTransform(cameraEntity, tr);
}

/** Camera + one directional light only (no meshes). */
void BuildMinimalScene(eyelib::api::world::World& world, ::eyelib::world::EntityId* outCamera) {
    using namespace eyelib::world::components;
    using eyelib::math::Vec3;
    using eyelib::math::Quat;

    ::eyelib::world::World* internal = world.GetInternal();
    if (!internal || !outCamera) return;

    auto& state = internal->GetState();
    auto& transformStore = state.GetTransformStore();

    ::eyelib::world::EntityId cam = world.CreateEntity("Camera");
    *outCamera = cam;
    Transform camTr;
    camTr.position = Vec3(0.f, 2.f, 10.f);
    camTr.rotation = Quat(1.f, 0.f, 0.f, 0.f);
    camTr.scale = Vec3(1.f, 1.f, 1.f);
    transformStore.Add(cam, camTr);
    Camera camComp;
    camComp.isPrimary = true;
    camComp.isPerspective = true;
    camComp.fov = 50.f;
    camComp.aspect = 16.f / 9.f;
    camComp.nearPlane = 0.1f;
    camComp.farPlane = 1000.f;
    world.SetCamera(cam, camComp);

    ::eyelib::world::EntityId lightEnt = world.CreateEntity("Light");
    Transform lightTr;
    lightTr.position = Vec3(0.f, 4.f, 0.f);
    lightTr.rotation = Quat(1.f, 0.f, 0.f, 0.f);
    lightTr.scale = Vec3(1.f, 1.f, 1.f);
    transformStore.Add(lightEnt, lightTr);
    Light lightComp;
    lightComp.type = Light::Type::Directional;
    lightComp.color = Vec3(1.f, 1.f, 1.f);
    lightComp.intensity = 0.5f;
    lightComp.range = 20.f;
    lightComp.castShadows = false;
    world.SetLight(lightEnt, lightComp);
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    AttachConsoleIfNeeded();

    std::string assetRoot = GetAssetRoot();

    eyelib::api::core::Engine::Config engineConfig;
    engineConfig.enableLogging = true;
    engineConfig.logFile = "TestForwardSkinned.log";
    engineConfig.headless = false;
    if (!eyelib::api::core::Engine::Initialize(engineConfig)) {
        std::fprintf(stderr, "Engine init failed.\n");
        return 4;
    }

    eyelib::api::world::World world;
    ::eyelib::world::EntityId cameraEntity = ::eyelib::world::InvalidEntity;
    BuildMinimalScene(world, &cameraEntity);

    eyelib::api::Window window;
    eyelib::api::WindowConfig wc;
    wc.title = "Skybox + Camera (right-drag to rotate)";
    wc.width = 1280;
    wc.height = 720;
    wc.vsync = true;
    wc.useNativeApiOnly = false;
    if (!window.Initialize(wc)) {
        eyelib::api::core::Log::Error("Window init failed.");
        eyelib::api::core::Engine::Shutdown();
        return 8;
    }

    openeye::player::WorldRenderBridge renderBridge;
    if (!renderBridge.Initialize(window, static_cast<uint32_t>(window.Width()),
                                 static_cast<uint32_t>(window.Height()),
                                 eyelib::render::RenderBackend::OpenGL)) {
        eyelib::api::core::Log::Error("Render init failed.");
        window.Shutdown();
        eyelib::api::core::Engine::Shutdown();
        return 9;
    }

    std::shared_ptr<eyelib::api::TextureCube> skybox = LoadSkybox(assetRoot);
    if (!skybox) {
        std::fprintf(stderr, "Skybox load failed. Run from project root or copy assets/ to exe dir.\n");
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
    frameSync.SetFenceWaiter([&renderBridge](void* f) { renderBridge.WaitAndDestroyFence(f); });

    OrbitState orbit;
    bool firstFrame = true;

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (cameraEntity != ::eyelib::world::InvalidEntity)
            UpdateOrbit(world, cameraEntity, orbit);

        int slot = frameSync.GetFillSlot();
        if (slot < 0) break;

        renderBridge.BuildScene(world, options, &animationBridge, &assetRegistry,
                                frameSync.GetSceneBuffer(slot), frameSync.GetFrameAllocator(slot));

        eyelib::api::RenderScene& scene = frameSync.GetSceneBuffer(slot);
        scene.skybox = skybox;

        window.RefreshFramebufferSize();
        int w = window.Width();
        int h = window.Height();
        if (w > 0 && h > 0) {
            window.ApplyVsync();
            renderBridge.ApplyResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            renderBridge.RenderSceneOnly(scene, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            renderBridge.Finish();
            window.SwapBuffers();
        }
        frameSync.ReleaseFrame(slot, nullptr);

        eyelib::api::Input::Update();
        firstFrame = false;
    }

    renderBridge.Shutdown();
    window.Shutdown();
    eyelib::api::core::Engine::Shutdown();
    return 0;
}
