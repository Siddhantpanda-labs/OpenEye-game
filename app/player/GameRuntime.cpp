#include "GameRuntime.h"

#include "AssetBinding.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"
#include "WorldAudio.h"

#include "eyelib/render/RenderBackend.h"

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
    Shutdown();
}

bool GameRuntime::Initialize(eyelib::api::world::World& world,
                             eyelib::api::AssetRegistry& assetRegistry,
                             const std::vector<std::string>& assetRoots,
                             eyelib::api::Window& window,
                             uint32_t width, uint32_t height,
                             eyelib::render::RenderBackend backend) {
    if (m_ready) {
        Shutdown();
    }

    m_world = &world;
    m_assetRegistry = &assetRegistry;
    m_assetRoots = assetRoots;
    m_options = {};
    m_options.assetRoots = assetRoots;
    m_options.fixedTimestep = 0.0f;
    m_options.maxSubsteps = 8;
    m_options.overlay = false;
    m_options.stats = false;

    BindAssets(world, assetRoots, assetRegistry, true);

    m_render = std::make_unique<WorldRenderBridge>();
    if (!m_render->Initialize(window, width, height, backend)) {
        m_render.reset();
        return false;
    }

    m_physics = std::make_unique<WorldPhysicsBridge>();
    if (!m_physics->Initialize()) {
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
    m_scripting.reset();
    m_animation.reset();
    if (m_render) {
        m_render->Shutdown();
        m_render.reset();
    }
    try {
        if (m_physics) {
            m_physics->Shutdown();
            m_physics.reset();
        }
    } catch (...) {}
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
    if (!m_ready || !m_world || !m_assetRegistry || !m_render) return nullptr;

    if (width != m_width || height != m_height) {
        m_render->ApplyResize(width, height);
        m_width = width;
        m_height = height;
    }
    if (m_width == 0 || m_height == 0) return m_render->GetOutputColorTextureNativeHandle();

    ResetScene();
    m_render->BuildScene(*m_world, m_options, m_animation.get(), m_assetRegistry, m_scene, &m_frameAlloc);
    m_render->RenderSceneOnly(m_scene, m_width, m_height);
    return m_render->GetOutputColorTextureNativeHandle();
}

void* GameRuntime::GetOutputColorTextureNativeHandle() const {
    return m_render ? m_render->GetOutputColorTextureNativeHandle() : nullptr;
}

} // namespace openeye::player
