#include "GameRuntime.h"

#include "AssetBinding.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"
#include "WorldAudio.h"

#include "api/render/RenderBackend.h"

#ifdef OPENEYE_BUILD_STREAMING
#include "app/streaming/VRStreamingServer.h"
#endif

namespace openeye::player {

void GameRuntime::ResetScene() {
    if (m_scene.opaque && m_scene.opaqueCount > 0) {
        for (size_t i = 0; i < m_scene.opaqueCount; ++i)
            m_scene.opaque[i].~Drawable();
    }
    if (m_scene.transparent && m_scene.transparentCount > 0) {
        for (size_t i = 0; i < m_scene.transparentCount; ++i)
            m_scene.transparent[i].~Drawable();
    }
    m_scene.opaque = nullptr;
    m_scene.opaqueCount = 0;
    m_scene.transparent = nullptr;
    m_scene.transparentCount = 0;
    m_scene.pointLights = nullptr;
    m_scene.pointLightCount = 0;
    m_scene.spotLights = nullptr;
    m_scene.spotLightCount = 0;
    m_frameAlloc.Reset();
}

GameRuntime::~GameRuntime() {
#ifdef OPENEYE_BUILD_STREAMING
    StopStreaming();
#endif
    Shutdown();
}

bool GameRuntime::Initialize(eyelib::api::world::World& world,
                             eyelib::api::AssetRegistry& assetRegistry,
                             const std::vector<std::string>& assetRoots,
                             eyelib::api::Window* window,
                             uint32_t width, uint32_t height,
                             eyelib::render::RenderBackend backend,
                             eyelib::api::RenderDevice sharedDevice,
                             const RuntimeOptions* options) {
    if (m_ready) {
        Shutdown();
    }

    m_world = &world;
    m_assetRegistry = &assetRegistry;
    m_assetRoots = assetRoots;
    m_options = {};
    m_options.assetRoots = assetRoots;
    m_options.fixedTimestep = options ? options->fixedTimestep : 0.0f;
    m_options.maxSubsteps = options ? options->maxSubsteps : 8;
    m_options.overlay = false;
    m_options.stats = false;

    const bool headless = (window == nullptr);
    const bool allowGpuAssets = !headless;
    m_assetBindStats = BindAssets(world, assetRoots, assetRegistry, allowGpuAssets);

    if (!headless) {
        // Lifetime domain: use sharedDevice (editor) when valid; otherwise create device from window.
        m_render = std::make_unique<WorldRenderBridge>();
        if (sharedDevice) {
            if (!m_render->Initialize(sharedDevice.Get(), width, height)) {
                m_render.reset();
                return false;
            }
        } else {
            if (!m_render->Initialize(*window, width, height, backend)) {
                m_render.reset();
                return false;
            }
        }
        assetRegistry.SetRenderDevice(m_render->GetDevice());
        m_render->InitializeDefaultResources();
    }

    world.FlushTransforms();

    m_physics = std::make_unique<WorldPhysicsBridge>();
    if (!m_physics->Initialize(eyelib::api::physics::PhysicsWorld::PhysicsBackendType::Jolt)) {
        Shutdown();
        return false;
    }
    if (!m_physics->BuildFromWorld(world)) {
        // continue
    }

    m_animation = std::make_unique<WorldAnimationBridge>();
    m_animation->BuildFromWorld(world, assetRegistry);

    m_scripting = std::make_unique<WorldScriptingBridge>();
    m_scripting->SetPhysicsBridge(m_physics.get());
    m_scripting->SetAnimationBridge(m_animation.get());
    m_scripting->BuildFromWorld(world, assetRoots);

    m_audio = std::make_unique<WorldAudioBridge>();
    if (!m_audio->Initialize()) {
        // continue without audio
    } else {
        m_audio->BuildFromWorld(world);
    }

    m_width = width;
    m_height = height;
    m_ready = true;
    return true;
}

void GameRuntime::Shutdown() {
    if (!m_ready) return;

    m_ready = false;
    if (m_audio) {
        m_audio->Shutdown();
        m_audio.reset();
    }
    // Disable scripting access to physics before tearing down physics (per physics_advice §4).
    if (m_scripting) {
        m_scripting->SetPhysicsBridge(nullptr);
    }
    m_scripting.reset();
    m_animation.reset();
    // Shutdown physics before render so simulation is torn down while device is still valid.
    try {
        if (m_physics) {
            m_physics->Shutdown();
            m_physics.reset();
        }
    } catch (...) {}
    if (m_render) {
        m_render->Shutdown();
        m_render.reset();
    }
    m_world = nullptr;
    m_assetRegistry = nullptr;
    m_assetRoots.clear();
}

void GameRuntime::Tick(float deltaSeconds) {
    if (!m_ready || !m_world) return;

    const float fixedStep = m_options.fixedTimestep > 0.0f ? m_options.fixedTimestep : 0.0f;
    m_physics->Step(*m_world, deltaSeconds, m_options.maxSubsteps, fixedStep);
    if (m_audio) {
        m_audio->Update(*m_world, deltaSeconds);
    }
    m_scripting->Update(*m_world, m_assetRoots, deltaSeconds);
    m_world->Update(deltaSeconds);
}

void* GameRuntime::Render(uint32_t width, uint32_t height) {
    if (!m_ready || !m_render) return nullptr;
    if (!m_world || !m_assetRegistry) return nullptr;

    if (width != m_width || height != m_height) {
        m_render->ApplyResize(width, height);
        m_width = width;
        m_height = height;
    }
    if (m_width == 0 || m_height == 0) return m_render->GetOutputColorTextureNativeHandle();

    ResetScene();
    m_render->BuildScene(*m_world, m_options, m_animation.get(), m_assetRegistry, m_scene, &m_frameAlloc);
    m_render->RenderSceneOnly(m_scene, m_width, m_height);

#ifdef OPENEYE_BUILD_STREAMING
    if (m_streaming && m_streaming->IsRunning()) {
        m_streaming->Tick();
        if (m_streaming->GetClientCount() > 0) {
            const size_t need = static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4u;
            if (m_streamingReadbackBuffer.size() < need)
                m_streamingReadbackBuffer.resize(need);
            if (m_render->ReadOutputPixels(m_width, m_height, m_streamingReadbackBuffer.data()))
                m_streaming->SendFrame(m_streamingReadbackBuffer.data(), m_width, m_height, 75);
        }
    }
#endif

    return m_render->GetOutputColorTextureNativeHandle();
}

void* GameRuntime::GetOutputColorTextureNativeHandle() const {
    return m_render ? m_render->GetOutputColorTextureNativeHandle() : nullptr;
}

void GameRuntime::ReloadShaders() {
    if (m_render) {
        m_render->ReloadShaders();
    }
}

#ifdef OPENEYE_BUILD_STREAMING
bool GameRuntime::StartStreaming(uint16_t streamPort) {
    if (IsHeadless()) return false;
    if (m_streaming && m_streaming->IsRunning()) return true;
    if (!m_streaming) m_streaming = std::make_unique<eyelib::app::VRStreamingServer>();
    if (m_streamingGyroCallback)
        m_streaming->SetGyroCallback(m_streamingGyroCallback);
    return m_streaming->Start(streamPort);
}

void GameRuntime::StopStreaming() {
    if (m_streaming)
        m_streaming->Stop();
}

bool GameRuntime::IsStreaming() const {
    return m_streaming && m_streaming->IsRunning();
}

size_t GameRuntime::GetStreamingClientCount() const {
    return m_streaming && m_streaming->IsRunning() ? m_streaming->GetClientCount() : 0u;
}

void GameRuntime::SetStreamingGyroCallback(std::function<void(float yaw, float pitch, float roll)> cb) {
    m_streamingGyroCallback = std::move(cb);
    if (m_streaming)
        m_streaming->SetGyroCallback(m_streamingGyroCallback);
}
#endif

} // namespace openeye::player
