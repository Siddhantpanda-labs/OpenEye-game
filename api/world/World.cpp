#include "World.h"
#include "Serialization.h"

#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Animator.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Script.h"
#include "eyelib/world/components/Transform.h"

#include <unordered_set>

namespace eyelib {
namespace api {
namespace world {

class World::Impl {
public:
    ::eyelib::world::World world;
};

World::World()
    : m_impl(new Impl()) {}

World::~World() {
    delete m_impl;
    m_impl = nullptr;
}

void World::SubmitCommand(std::unique_ptr<::eyelib::world::Command> command) {
    if (!m_impl) {
        return;
    }
    m_impl->world.SubmitCommand(std::move(command));
}

void World::ApplyPendingCommands() {
    if (!m_impl) {
        return;
    }
    m_impl->world.ApplyPendingCommands();
}

void World::ApplyTransaction(::eyelib::world::Transaction&& transaction) {
    if (!m_impl) {
        return;
    }
    m_impl->world.ApplyTransaction(std::move(transaction));
}

void World::Update() {
    if (!m_impl) {
        return;
    }
    m_impl->world.Update();
}

void World::Update(float deltaSeconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Update(deltaSeconds);
}

void World::SetFixedTimestep(float seconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetFixedTimestep(seconds);
}

float World::GetFixedTimestep() const {
    if (!m_impl) {
        return 0.0f;
    }
    return m_impl->world.Scheduler().GetFixedTimestep();
}

void World::SetMaxSubsteps(int maxSubsteps) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetMaxSubsteps(maxSubsteps);
}

int World::GetMaxSubsteps() const {
    if (!m_impl) {
        return 0;
    }
    return m_impl->world.Scheduler().GetMaxSubsteps();
}

void World::SetPaused(bool paused) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetPaused(paused);
}

void World::StepOnce() {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().StepOnce();
}

void World::SetTimeScale(float scale) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetTimeScale(scale);
}

float World::GetTimeScale() const {
    if (!m_impl) {
        return 0.0f;
    }
    return m_impl->world.Scheduler().GetTimeScale();
}

void World::SetTimeMode(::eyelib::world::WorldScheduler::TimeMode mode) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetTimeMode(mode);
}

::eyelib::world::WorldScheduler::TimeMode World::GetTimeMode() const {
    if (!m_impl) {
        return ::eyelib::world::WorldScheduler::TimeMode::Realtime;
    }
    return m_impl->world.Scheduler().GetTimeMode();
}

void World::SetReplayDelta(float seconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetReplayDelta(seconds);
}

std::vector<World::AssetReference> World::CollectAssetReferences() const {
    if (!m_impl) {
        return {};
    }

    std::vector<AssetReference> refs;
    std::unordered_set<std::string> seen;
    auto addRef = [&](const std::string& id, AssetRefKind kind) {
        if (id.empty()) {
            return;
        }
        const std::string key = std::to_string(static_cast<int>(kind)) + ":" + id;
        if (seen.insert(key).second) {
            refs.push_back({ id, kind });
        }
    };

    const auto& state = m_impl->world.GetState();

    const auto& renderables = state.Components<::eyelib::world::components::Renderable>().All();
    for (const auto& [entity, renderable] : renderables) {
        (void)entity;
        addRef(renderable.meshAssetId, AssetRefKind::Mesh);
        addRef(renderable.materialAssetId, AssetRefKind::Material);
        for (const auto& lod : renderable.lodMeshAssetIds) {
            addRef(lod, AssetRefKind::Mesh);
        }
    }

    const auto& meshRenderers = state.Components<::eyelib::world::components::MeshRenderer>().All();
    for (const auto& [entity, renderer] : meshRenderers) {
        (void)entity;
        addRef(renderer.meshAssetId, AssetRefKind::Mesh);
        addRef(renderer.materialAssetId, AssetRefKind::Material);
    }

    const auto& animators = state.Components<::eyelib::world::components::Animator>().All();
    for (const auto& [entity, animator] : animators) {
        (void)entity;
        addRef(animator.animatorAssetId, AssetRefKind::Animation);
        addRef(animator.skeletonAssetId, AssetRefKind::Skeleton);
    }

    const auto& scripts = state.Components<::eyelib::world::components::Script>().All();
    for (const auto& [entity, script] : scripts) {
        (void)entity;
        addRef(script.scriptName, AssetRefKind::Script);
    }

    return refs;
}

std::vector<World::TransformEntry> World::GetTransforms() const {
    std::vector<TransformEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Transform>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::RenderableEntry> World::GetRenderables() const {
    std::vector<RenderableEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Renderable>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::MeshRendererEntry> World::GetMeshRenderers() const {
    std::vector<MeshRendererEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::MeshRenderer>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::CameraEntry> World::GetCameras() const {
    std::vector<CameraEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Camera>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::LightEntry> World::GetLights() const {
    std::vector<LightEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Light>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::RigidBodyEntry> World::GetRigidBodies() const {
    std::vector<RigidBodyEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::RigidBody>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::ColliderEntry> World::GetColliders() const {
    std::vector<ColliderEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Collider>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::AnimatorEntry> World::GetAnimators() const {
    std::vector<AnimatorEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Animator>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<World::ScriptEntry> World::GetScripts() const {
    std::vector<ScriptEntry> out;
    if (!m_impl) {
        return out;
    }
    const auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Script>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

bool World::SetTransform(::eyelib::world::EntityId entity,
                         const ::eyelib::world::components::Transform& transform) {
    if (!m_impl) {
        return false;
    }
    auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Transform>();
    auto* entry = store.TryGet(entity);
    if (!entry) {
        return false;
    }
    *entry = transform;
    return true;
}

bool World::SetRigidBody(::eyelib::world::EntityId entity,
                         const ::eyelib::world::components::RigidBody& rigidBody) {
    if (!m_impl) {
        return false;
    }
    auto& store = m_impl->world.GetState().Components<::eyelib::world::components::RigidBody>();
    auto* entry = store.TryGet(entity);
    if (!entry) {
        return false;
    }
    *entry = rigidBody;
    return true;
}

bool World::SetScript(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Script& script) {
    if (!m_impl) {
        return false;
    }
    auto& store = m_impl->world.GetState().Components<::eyelib::world::components::Script>();
    auto* entry = store.TryGet(entity);
    if (!entry) {
        return false;
    }
    *entry = script;
    return true;
}

bool World::LoadScene(const std::string& path) {
    if (!m_impl) {
        return false;
    }
    Serializer serializer;
    return serializer.Load(m_impl->world.GetState(), path);
}

size_t World::GetEntityCount() const {
    if (!m_impl) {
        return 0;
    }
    return m_impl->world.GetState().Entities().GetAliveEntities().size();
}

bool World::UndoLastTransaction() {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.UndoLastTransaction();
}

bool World::RedoLastTransaction() {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.RedoLastTransaction();
}

::eyelib::world::World* World::GetInternal() const {
    return m_impl ? &m_impl->world : nullptr;
}

} // namespace world
} // namespace api
} // namespace eyelib
