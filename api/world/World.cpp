#include "World.h"
#include "Observation.h"
#include "Serialization.h"
#include "Commands.h"

#include "eyelib/world/Commands/CommandTypes.h"

#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/WorldGraph/Tags.h"
#include "eyelib/world/Serialization/ComponentSerializer.h"
#include <json.hpp>
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Animator.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Script.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/observation/WorldObservation.h"

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

World::ApplySyncResult World::ApplyCommandSync(std::unique_ptr<::eyelib::world::Command> command) {
    ApplySyncResult out{};
    if (!m_impl) {
        return out;
    }
    auto r = m_impl->world.ApplyCommandSync(std::move(command));
    out.success = r.success;
    out.createdEntity = r.createdEntity;
    return out;
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

void World::SetGravity(const ::eyelib::math::Vec3& gravity) {
    if (!m_impl) return;
    m_impl->world.SetGravity(gravity);
}

::eyelib::math::Vec3 World::GetGravity() const {
    if (!m_impl) return ::eyelib::math::Vec3(0.f, -9.8f, 0.f);
    return m_impl->world.GetGravity();
}

void World::AddForce(::eyelib::world::EntityId entity, const ::eyelib::math::Vec3& force, ::eyelib::world::ForceMode mode) {
    if (!m_impl) return;
    m_impl->world.AddForce(entity, force, mode);
}

void World::AddImpulse(::eyelib::world::EntityId entity, const ::eyelib::math::Vec3& impulse) {
    if (!m_impl) return;
    m_impl->world.AddImpulse(entity, impulse);
}

void World::AddTorque(::eyelib::world::EntityId entity, const ::eyelib::math::Vec3& torque) {
    if (!m_impl) return;
    m_impl->world.AddTorque(entity, torque);
}

void World::AddAngularImpulse(::eyelib::world::EntityId entity, const ::eyelib::math::Vec3& impulse) {
    if (!m_impl) return;
    m_impl->world.AddAngularImpulse(entity, impulse);
}

double World::GetSimulationTime() const {
    if (!m_impl) {
        return 0.0;
    }
    return m_impl->world.GetSimulationTime();
}

double World::GetSimulationAccumulator() const {
    if (!m_impl) {
        return 0.0;
    }
    return m_impl->world.Scheduler().GetAccumulator();
}

std::optional<::eyelib::world::components::Transform> World::GetInterpolatedTransform(::eyelib::world::EntityId entity, float alpha) const {
    if (!m_impl) {
        return std::nullopt;
    }
    return m_impl->world.GetInterpolatedTransform(entity, alpha);
}

std::vector<::eyelib::world::EventLogEntry> World::GetEventsSince(uint64_t sinceSequenceId) const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetEventsSince(sinceSequenceId);
}

std::vector<::eyelib::world::EventLogEntry> World::GetEventsSinceTime(double sinceTime) const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetEventsSinceTime(sinceTime);
}

std::vector<::eyelib::world::ContactEvent> World::GetAndClearContactEvents() {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetAndClearContactEvents();
}

void World::SetPostPhysicsStepCallback(std::function<void()> fn) {
    if (!m_impl) {
        return;
    }
    m_impl->world.SetPostPhysicsStepCallback(std::move(fn));
}

World::ContactListenerId World::AddContactListener(ContactListener listener) {
    if (!m_impl) {
        return 0;
    }
    return m_impl->world.AddContactListener(std::move(listener));
}

void World::RemoveContactListener(ContactListenerId id) {
    if (!m_impl) {
        return;
    }
    m_impl->world.RemoveContactListener(id);
}

void World::DrainAndDispatchContactEvents() {
    if (!m_impl) {
        return;
    }
    m_impl->world.DrainAndDispatchContactEvents();
}

std::vector<std::string> World::GetSystemList() const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetSystemList();
}

std::vector<std::string> World::GetRegionEnabledSystems(::eyelib::world::EntityId regionEntityId) const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetRegionEnabledSystems(regionEntityId);
}

::eyelib::world::EntityId World::CreateEntity(const std::string& name) {
    if (!m_impl) {
        return ::eyelib::world::InvalidEntity;
    }
    auto r = ApplyCommandSync(eyelib::api::world::CreateEntity(name));
    return (r.success && r.createdEntity) ? *r.createdEntity : ::eyelib::world::InvalidEntity;
}

bool World::DestroyEntity(::eyelib::world::EntityId entity) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::DestroyEntity(entity)).success;
}

bool World::SetParent(::eyelib::world::EntityId child, ::eyelib::world::EntityId parent, bool preserveWorldTransform) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::ReparentEntity(child, parent, preserveWorldTransform)).success;
}

void World::ClearParent(::eyelib::world::EntityId child, bool preserveWorldTransform) {
    if (!m_impl) {
        return;
    }
    ApplyCommandSync(eyelib::api::world::ReparentEntity(child, ::eyelib::world::InvalidEntity, preserveWorldTransform));
}

::eyelib::world::EntityId World::GetParent(::eyelib::world::EntityId child) const {
    if (!m_impl) {
        return ::eyelib::world::InvalidEntity;
    }
    return m_impl->world.GetState().GraphHierarchy().GetParent(child);
}

std::vector<::eyelib::world::EntityId> World::GetChildren(::eyelib::world::EntityId parent) const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetState().GraphHierarchy().GetChildren(parent);
}

std::vector<::eyelib::world::EntityId> World::GetAncestors(::eyelib::world::EntityId entity) const {
    if (!m_impl) {
        return {};
    }
    std::vector<::eyelib::world::EntityId> ancestors;
    const auto& hierarchy = m_impl->world.GetState().GraphHierarchy();
    ::eyelib::world::EntityId current = hierarchy.GetParent(entity);
    while (current != ::eyelib::world::InvalidEntity) {
        ancestors.push_back(current);
        current = hierarchy.GetParent(current);
    }
    return ancestors;
}

std::vector<::eyelib::world::EntityId> World::GetDescendants(::eyelib::world::EntityId entity) const {
    if (!m_impl) {
        return {};
    }
    std::vector<::eyelib::world::EntityId> result;
    std::vector<::eyelib::world::EntityId> stack;
    stack.push_back(entity);
    const auto& hierarchy = m_impl->world.GetState().GraphHierarchy();
    while (!stack.empty()) {
        const auto current = stack.back();
        stack.pop_back();
        for (const auto& child : hierarchy.GetChildren(current)) {
            result.push_back(child);
            stack.push_back(child);
        }
    }
    return result;
}

void World::AddTag(::eyelib::world::EntityId entity, const std::string& tag) {
    if (!m_impl || tag.empty()) {
        return;
    }
    SubmitCommand(eyelib::api::world::AddTag(entity, tag));
    ApplyPendingCommands();
}

void World::RemoveTag(::eyelib::world::EntityId entity, const std::string& tag) {
    if (!m_impl || tag.empty()) {
        return;
    }
    SubmitCommand(eyelib::api::world::RemoveTag(entity, tag));
    ApplyPendingCommands();
}

bool World::HasTag(::eyelib::world::EntityId entity, const std::string& tag) const {
    if (!m_impl || tag.empty()) {
        return false;
    }
    const auto& state = m_impl->world.GetState();
    if (!state.Entities().IsAlive(entity)) {
        return false;
    }
    return state.GraphTags().Has(entity, tag);
}

std::vector<std::string> World::GetTags(::eyelib::world::EntityId entity) const {
    if (!m_impl) {
        return {};
    }
    const auto& state = m_impl->world.GetState();
    if (!state.Entities().IsAlive(entity)) {
        return {};
    }
    const auto& allTags = state.GraphTags().All();
    auto it = allTags.find(entity);
    if (it == allTags.end()) {
        return {};
    }
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<::eyelib::world::EntityId> World::GetEntitiesWithTag(const std::string& tag) const {
    if (!m_impl || tag.empty()) {
        return {};
    }
    std::vector<::eyelib::world::EntityId> result;
    const auto& state = m_impl->world.GetState();
    const auto& allTags = state.GraphTags().All();
    for (const auto& [entity, tagSet] : allTags) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        if (tagSet.find(tag) != tagSet.end()) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<::eyelib::world::EntityId> World::GetEntitiesWithTags(
    const std::vector<std::string>& tags) const {
    if (!m_impl || tags.empty()) {
        return {};
    }
    std::unordered_set<std::string> required(tags.begin(), tags.end());
    std::vector<::eyelib::world::EntityId> result;
    const auto& state = m_impl->world.GetState();
    const auto& allTags = state.GraphTags().All();
    for (const auto& [entity, tagSet] : allTags) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        bool ok = true;
        for (const auto& tag : required) {
            if (tagSet.find(tag) == tagSet.end()) {
                ok = false;
                break;
            }
        }
        if (ok) {
            result.push_back(entity);
        }
    }
    return result;
}

bool World::SetMetadata(::eyelib::world::EntityId entity, const std::string& name, uint32_t flags) {
    if (!m_impl) {
        return false;
    }
    SubmitCommand(eyelib::api::world::SetMetadata(entity, name, flags));
    ApplyPendingCommands();
    return true;
}

bool World::GetMetadata(::eyelib::world::EntityId entity, std::string& name, uint32_t& flags) const {
    if (!m_impl) {
        return false;
    }
    const auto& state = m_impl->world.GetState();
    if (!state.Entities().IsAlive(entity)) {
        return false;
    }
    auto entry = state.GraphMetadata().Get(entity);
    if (!entry.has_value()) {
        return false;
    }
    name = entry->name;
    flags = entry->flags;
    return true;
}

bool World::ClearMetadata(::eyelib::world::EntityId entity) {
    if (!m_impl) {
        return false;
    }
    SubmitCommand(eyelib::api::world::ClearMetadata(entity));
    ApplyPendingCommands();
    return true;
}

bool World::GetEntityName(::eyelib::world::EntityId entity, std::string& name) const {
    if (!m_impl) {
        return false;
    }
    const auto& state = m_impl->world.GetState();
    if (!state.Entities().IsAlive(entity)) {
        return false;
    }
    auto entry = state.GraphMetadata().Get(entity);
    if (!entry.has_value()) {
        return false;
    }
    name = entry->name;
    return true;
}

bool World::SetEntityName(::eyelib::world::EntityId entity, const std::string& name) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetName(entity, name)).success;
}

::eyelib::world::EntityId World::FindEntityByName(const std::string& name) const {
    if (!m_impl || name.empty()) {
        return ::eyelib::world::InvalidEntity;
    }
    const auto& state = m_impl->world.GetState();
    const auto& data = state.GraphMetadata().All();
    for (const auto& [entity, entry] : data) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        if (entry.name == name) {
            return entity;
        }
    }
    return ::eyelib::world::InvalidEntity;
}

std::vector<::eyelib::world::EntityId> World::FindEntitiesByName(const std::string& name) const {
    std::vector<::eyelib::world::EntityId> result;
    if (!m_impl || name.empty()) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    const auto& data = state.GraphMetadata().All();
    for (const auto& [entity, entry] : data) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        if (entry.name == name) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<::eyelib::world::EntityId> World::FindEntitiesByNameContains(const std::string& namePart) const {
    std::vector<::eyelib::world::EntityId> result;
    if (!m_impl || namePart.empty()) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    const auto& data = state.GraphMetadata().All();
    for (const auto& [entity, entry] : data) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        if (entry.name.find(namePart) != std::string::npos) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<::eyelib::world::EntityId> World::FindEntitiesByFlags(uint32_t flagsMask, bool exactMatch) const {
    std::vector<::eyelib::world::EntityId> result;
    if (!m_impl || flagsMask == 0) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    const auto& data = state.GraphMetadata().All();
    for (const auto& [entity, entry] : data) {
        if (!state.Entities().IsAlive(entity)) {
            continue;
        }
        if (exactMatch) {
            if (entry.flags == flagsMask) {
                result.push_back(entity);
            }
        } else {
            if ((entry.flags & flagsMask) == flagsMask) {
                result.push_back(entity);
            }
        }
    }
    return result;
}

std::vector<::eyelib::world::EntityId> World::GetEntities() const {
    if (!m_impl) {
        return {};
    }
    return m_impl->world.GetState().Entities().GetAliveEntities();
}

std::vector<::eyelib::world::EntityId> World::GetEntitiesWithComponent(
    const std::string& componentName) const {
    std::vector<::eyelib::world::EntityId> result;
    if (!m_impl || componentName.empty()) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    const auto* desc = ::eyelib::world::ComponentSerializer::Get().GetDescriptorByName(componentName);
    if (!desc) {
        return result;
    }
    const auto* store = state.FindComponentStore(desc->typeKey);
    if (!store) {
        return result;
    }
    for (const auto& entity : state.Entities().GetAliveEntities()) {
        if (store->Has(entity)) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<::eyelib::world::EntityId> World::GetEntitiesWithComponents(
    const std::vector<std::string>& componentNames) const {
    std::vector<::eyelib::world::EntityId> result;
    if (!m_impl || componentNames.empty()) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    std::vector<const ::eyelib::world::IComponentStore*> stores;
    stores.reserve(componentNames.size());
    for (const auto& name : componentNames) {
        const auto* desc = ::eyelib::world::ComponentSerializer::Get().GetDescriptorByName(name);
        if (!desc) {
            return {};
        }
        const auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return {};
        }
        stores.push_back(store);
    }
    for (const auto& entity : state.Entities().GetAliveEntities()) {
        bool ok = true;
        for (const auto* store : stores) {
            if (!store->Has(entity)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<std::string> World::GetComponentNames(::eyelib::world::EntityId entity) const {
    std::vector<std::string> result;
    if (!m_impl) {
        return result;
    }
    const auto& state = m_impl->world.GetState();
    if (!state.Entities().IsAlive(entity)) {
        return result;
    }
    const auto& stores = state.ComponentStores();
    result.reserve(stores.size());
    for (const auto& [typeKey, store] : stores) {
        if (!store || !store->Has(entity)) {
            continue;
        }
        const auto* desc = ::eyelib::world::ComponentSerializer::Get().GetDescriptor(typeKey);
        if (!desc || desc->name.empty()) {
            continue;
        }
        result.push_back(desc->name);
    }
    return result;
}

bool World::GetComponentJson(::eyelib::world::EntityId entity,
                             const std::string& componentName,
                             std::string& outJson) const {
    if (!m_impl || componentName.empty()) {
        return false;
    }
    const auto& state = m_impl->world.GetState();
    const auto* desc = ::eyelib::world::ComponentSerializer::Get().GetDescriptorByName(componentName);
    if (!desc) {
        return false;
    }
    const auto* store = state.FindComponentStore(desc->typeKey);
    if (!store || !store->Has(entity)) {
        return false;
    }
    nlohmann::json data = desc->serializeOne(*store, entity);
    outJson = data.dump();
    return true;
}

bool World::SetComponentJson(::eyelib::world::EntityId entity,
                             const std::string& componentName,
                             const std::string& json) {
    if (!m_impl || componentName.empty()) {
        return false;
    }
    nlohmann::json data = nlohmann::json::parse(json, nullptr, false);
    if (data.is_discarded()) {
        return false;
    }
    SubmitCommand(std::make_unique<::eyelib::world::SetComponentDataByNameCommand>(entity, componentName, std::move(data)));
    ApplyPendingCommands();
    return true;
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

bool World::IsAlive(::eyelib::world::EntityId entity) const {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.GetState().Entities().IsAlive(entity);
}

bool World::GetTransform(::eyelib::world::EntityId entity,
                         ::eyelib::world::components::Transform& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* transform = m_impl->world.GetState()
        .GetTransformStore()
        .TryGet(entity);
    if (!transform) {
        return false;
    }
    out = *transform;
    return true;
}

bool World::GetCamera(::eyelib::world::EntityId entity,
                      ::eyelib::world::components::Camera& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* camera = m_impl->world.GetState()
        .Components<::eyelib::world::components::Camera>()
        .TryGet(entity);
    if (!camera) {
        return false;
    }
    out = *camera;
    return true;
}

bool World::GetLight(::eyelib::world::EntityId entity,
                     ::eyelib::world::components::Light& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* light = m_impl->world.GetState()
        .Components<::eyelib::world::components::Light>()
        .TryGet(entity);
    if (!light) {
        return false;
    }
    out = *light;
    return true;
}

bool World::GetRenderable(::eyelib::world::EntityId entity,
                          ::eyelib::world::components::Renderable& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* renderable = m_impl->world.GetState()
        .Components<::eyelib::world::components::Renderable>()
        .TryGet(entity);
    if (!renderable) {
        return false;
    }
    out = *renderable;
    return true;
}

bool World::GetMeshRenderer(::eyelib::world::EntityId entity,
                            ::eyelib::world::components::MeshRenderer& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* renderer = m_impl->world.GetState()
        .Components<::eyelib::world::components::MeshRenderer>()
        .TryGet(entity);
    if (!renderer) {
        return false;
    }
    out = *renderer;
    return true;
}

bool World::GetCollider(::eyelib::world::EntityId entity,
                        ::eyelib::world::components::Collider& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* collider = m_impl->world.GetState()
        .Components<::eyelib::world::components::Collider>()
        .TryGet(entity);
    if (!collider) {
        return false;
    }
    out = *collider;
    return true;
}

bool World::GetScript(::eyelib::world::EntityId entity,
                      ::eyelib::world::components::Script& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* script = m_impl->world.GetState()
        .Components<::eyelib::world::components::Script>()
        .TryGet(entity);
    if (!script) {
        return false;
    }
    out = *script;
    return true;
}

bool World::GetRigidBody(::eyelib::world::EntityId entity,
                         ::eyelib::world::components::RigidBody& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* rigidBody = m_impl->world.GetState()
        .Components<::eyelib::world::components::RigidBody>()
        .TryGet(entity);
    if (!rigidBody) {
        return false;
    }
    out = *rigidBody;
    return true;
}

bool World::GetAnimator(::eyelib::world::EntityId entity,
                        ::eyelib::world::components::Animator& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* animator = m_impl->world.GetState()
        .Components<::eyelib::world::components::Animator>()
        .TryGet(entity);
    if (!animator) {
        return false;
    }
    out = *animator;
    return true;
}

bool World::GetName(::eyelib::world::EntityId entity,
                    ::eyelib::world::components::Name& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* name = m_impl->world.GetState()
        .Components<::eyelib::world::components::Name>()
        .TryGet(entity);
    if (!name) {
        return false;
    }
    out = *name;
    return true;
}

bool World::GetRegion(::eyelib::world::EntityId entity,
                      ::eyelib::world::components::Region& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* region = m_impl->world.GetState()
        .Components<::eyelib::world::components::Region>()
        .TryGet(entity);
    if (!region) {
        return false;
    }
    out = *region;
    return true;
}

bool World::GetRegionMembership(::eyelib::world::EntityId entity,
                                ::eyelib::world::components::RegionMembership& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* membership = m_impl->world.GetState()
        .Components<::eyelib::world::components::RegionMembership>()
        .TryGet(entity);
    if (!membership) {
        return false;
    }
    out = *membership;
    return true;
}

bool World::GetAABB(::eyelib::world::EntityId entity,
                    ::eyelib::world::components::AABB& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* aabb = m_impl->world.GetState()
        .Components<::eyelib::world::components::AABB>()
        .TryGet(entity);
    if (!aabb) {
        return false;
    }
    out = *aabb;
    return true;
}

bool World::GetSystemBindings(::eyelib::world::EntityId entity,
                              ::eyelib::world::components::SystemBindings& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* bindings = m_impl->world.GetState()
        .Components<::eyelib::world::components::SystemBindings>()
        .TryGet(entity);
    if (!bindings) {
        return false;
    }
    out = *bindings;
    return true;
}

bool World::GetActiveSystems(::eyelib::world::EntityId entity,
                             ::eyelib::world::components::ActiveSystems& out) const {
    if (!m_impl) {
        return false;
    }
    const auto* systems = m_impl->world.GetState()
        .Components<::eyelib::world::components::ActiveSystems>()
        .TryGet(entity);
    if (!systems) {
        return false;
    }
    out = *systems;
    return true;
}

bool World::SetTransform(::eyelib::world::EntityId entity,
                         const ::eyelib::world::components::Transform& transform) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetTransform(entity, transform)).success;
}

bool World::SetCamera(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Camera& camera) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, camera)).success;
}

bool World::SetLight(::eyelib::world::EntityId entity,
                     const ::eyelib::world::components::Light& light) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, light)).success;
}

bool World::SetRenderable(::eyelib::world::EntityId entity,
                          const ::eyelib::world::components::Renderable& renderable) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, renderable)).success;
}

bool World::SetMeshRenderer(::eyelib::world::EntityId entity,
                            const ::eyelib::world::components::MeshRenderer& renderer) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, renderer)).success;
}

bool World::SetCollider(::eyelib::world::EntityId entity,
                        const ::eyelib::world::components::Collider& collider) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, collider)).success;
}

bool World::SetScript(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Script& script) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, script)).success;
}

bool World::SetRigidBody(::eyelib::world::EntityId entity,
                         const ::eyelib::world::components::RigidBody& rigidBody) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, rigidBody)).success;
}

bool World::SetAnimator(::eyelib::world::EntityId entity,
                        const ::eyelib::world::components::Animator& animator) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, animator)).success;
}

bool World::SetName(::eyelib::world::EntityId entity,
                    const ::eyelib::world::components::Name& name) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, name)).success;
}

bool World::SetRegion(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Region& region) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetRegionData(entity, region)).success;
}

bool World::SetRegionMembership(::eyelib::world::EntityId entity,
                                const ::eyelib::world::components::RegionMembership& membership) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, membership)).success;
}

bool World::SetAABB(::eyelib::world::EntityId entity,
                    const ::eyelib::world::components::AABB& aabb) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, aabb)).success;
}

bool World::SetSystemBindings(::eyelib::world::EntityId entity,
                              const ::eyelib::world::components::SystemBindings& bindings) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, bindings)).success;
}

bool World::SetActiveSystems(::eyelib::world::EntityId entity,
                             const ::eyelib::world::components::ActiveSystems& systems) {
    if (!m_impl) {
        return false;
    }
    return ApplyCommandSync(eyelib::api::world::SetComponentData(entity, systems)).success;
}

bool World::LoadScene(const std::string& path) {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.ApplyCommandSync(std::make_unique<::eyelib::world::LoadSceneCommand>(path)).success;
}

bool World::SaveScene(const std::string& path) const {
    if (!m_impl) {
        return false;
    }
    Serializer serializer;
    return serializer.Save(m_impl->world.GetState(), path);
}

size_t World::GetEntityCount() const {
    if (!m_impl) {
        return 0;
    }
    return m_impl->world.GetState().Entities().GetAliveEntities().size();
}

std::string World::GetSkyboxAssetId() const {
    if (!m_impl) return {};
    return m_impl->world.GetState().GetSkyboxAssetId();
}

void World::SetSkyboxAssetId(const std::string& assetId) {
    if (m_impl) {
        m_impl->world.GetState().SetSkyboxAssetId(assetId);
    }
}

void World::CaptureRenderSnapshot(RenderSnapshot& out) const {
    out.renderables.clear();
    out.meshRenderers.clear();
    out.cameras.clear();
    out.lights.clear();
    out.rigidBodies.clear();
    out.colliders.clear();
    out.transformCache.clear();
    out.transforms.clear();
    out.transformEntityIds.clear();
    out.transformPositions.clear();
    out.transformRotations.clear();
    out.transformScales.clear();
    if (!m_impl) return;
    ::eyelib::world::WorldObservation obs(m_impl->world.GetView());
    const auto transforms = obs.GetTransformsView();
    for (const auto& [entity, t] : transforms) {
        out.transformCache[entity] = t;
        out.transforms.push_back({ entity, t });
        out.transformEntityIds.push_back(entity);
        out.transformPositions.push_back(t.position);
        out.transformRotations.push_back(t.rotation);
        out.transformScales.push_back(t.scale);
    }
    for (const auto& [entity, comp] : obs.GetRenderablesView())
        out.renderables.push_back({ entity, comp });
    for (const auto& [entity, comp] : obs.GetMeshRenderersView())
        out.meshRenderers.push_back({ entity, comp });
    for (const auto& [entity, comp] : obs.GetCamerasView())
        out.cameras.push_back({ entity, comp });
    for (const auto& [entity, comp] : obs.GetLightsView())
        out.lights.push_back({ entity, comp });
    for (const auto& [entity, comp] : obs.GetRigidBodiesView())
        out.rigidBodies.push_back({ entity, comp });
    for (const auto& [entity, comp] : obs.GetCollidersView())
        out.colliders.push_back({ entity, comp });
}

void World::SetFrameSnapshot(const RenderSnapshot* snapshot) {
    m_frameSnapshot = snapshot;
}

void World::SetCaptureSnapshotCallback(std::function<void()> fn) {
    if (!m_impl) return;
    m_impl->world.SetCaptureSnapshotCallback(std::move(fn));
}

void World::FlushSimulation() {
    if (!m_impl) return;
    m_impl->world.FlushSimulation();
}

void World::FlushTransforms() {
    if (!m_impl) return;
    m_impl->world.FlushTransforms();
}

std::optional<::eyelib::world::components::Transform> World::RenderSnapshot::GetTransform(::eyelib::world::EntityId entity) const {
    auto it = transformCache.find(entity);
    if (it != transformCache.end()) return it->second;
    return std::nullopt;
}

Observation World::GetObservation() const {
    if (!m_impl) {
        return Observation(::eyelib::world::World());
    }
    if (m_frameSnapshot) {
        return Observation(*m_frameSnapshot);
    }
    return Observation(m_impl->world);
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
