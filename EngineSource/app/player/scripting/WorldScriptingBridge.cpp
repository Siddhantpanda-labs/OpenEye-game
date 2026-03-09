#include "WorldScripting.h"

#include "api/core/All.h"
#include "api/platform/All.h"

#include "scripting/WorldLuaBindings.h"

namespace openeye::player {

namespace {

std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    const char last = base.back();
    if (last == '/' || last == '\\') {
        return base + leaf;
    }
    return base + "/" + leaf;
}

} // namespace

std::string WorldScriptingBridge::ResolveScriptPath(const std::string& name,
                                                    const std::vector<std::string>& assetRoots) const {
    using eyelib::api::platform::filesystem::FileExists;
    if (name.empty()) {
        return {};
    }
    if (FileExists(name)) {
        return name;
    }
    for (const auto& root : assetRoots) {
        const std::string candidate = JoinPath(root, name);
        if (FileExists(candidate)) {
            return candidate;
        }
    }
    return {};
}

void WorldScriptingBridge::UnloadBinding(const ::eyelib::world::EntityId& entity) {
    auto it = m_bindings.find(entity);
    if (it == m_bindings.end()) {
        return;
    }
    if (it->second.handle != eyelib::scripting::InvalidScriptHandle) {
        eyelib::api::Script::CallStop(it->second.handle);
    }
    if (it->second.handle != eyelib::scripting::InvalidScriptHandle) {
        eyelib::api::Script::Unload(it->second.handle);
    }
    m_bindings.erase(it);
}

void WorldScriptingBridge::LoadOrReload(eyelib::api::world::World& world,
                                        const eyelib::api::world::World::ScriptEntry& script,
                                        const std::vector<std::string>& assetRoots,
                                        bool forceReload) {
    if (!script.component.isEnabled || script.component.scriptName.empty()) {
        UnloadBinding(script.entity);
        return;
    }

    const std::string resolved = ResolveScriptPath(script.component.scriptName, assetRoots);
    if (resolved.empty()) {
        eyelib::api::core::Log::WarnF("Script not found: %s", script.component.scriptName.c_str());
        return;
    }

    auto it = m_bindings.find(script.entity);
    if (it == m_bindings.end()) {
        auto handle = eyelib::api::Script::Load(resolved);
        if (handle == eyelib::scripting::InvalidScriptHandle) {
            eyelib::api::core::Log::WarnF("Failed to load script: %s", resolved.c_str());
            return;
        }
        m_bindings.emplace(script.entity, ScriptBinding{ script.entity, handle, resolved });
        if (auto* state = eyelib::api::Script::GetState(handle)) {
            scripting::RegisterWorldBindings(state, &world, m_physics, m_animation);
            scripting::BindScriptInstance(state, eyelib::api::Script::GetName(handle), script.entity, script.component);
        }
        eyelib::api::Script::CallStart(handle);
        return;
    }

    if (forceReload || script.component.needsReload) {
        const bool ok = eyelib::api::Script::Reload(it->second.handle);
        if (!ok) {
            eyelib::api::core::Log::WarnF("Failed to reload script: %s", resolved.c_str());
        }
        it->second.resolvedPath = resolved;
        if (auto* state = eyelib::api::Script::GetState(it->second.handle)) {
            scripting::RegisterWorldBindings(state, &world, m_physics, m_animation);
            scripting::BindScriptInstance(state, eyelib::api::Script::GetName(it->second.handle),
                                          script.entity, script.component);
        }
        eyelib::api::Script::CallStart(it->second.handle);

        auto updated = script.component;
        updated.needsReload = false;
        world.SetScript(script.entity, updated);
    }
}

void WorldScriptingBridge::BuildFromWorld(eyelib::api::world::World& world,
                                          const std::vector<std::string>& assetRoots) {
    m_bindings.clear();
    const auto scripts = world.GetScripts();
    for (const auto& entry : scripts) {
        LoadOrReload(world, entry, assetRoots, false);
    }
}

void WorldScriptingBridge::Update(eyelib::api::world::World& world,
                                  const std::vector<std::string>& assetRoots,
                                  float deltaTime) {
    const auto scripts = world.GetScripts();
    std::unordered_map<::eyelib::world::EntityId, bool, ::eyelib::world::EntityIdHash> active;
    active.reserve(scripts.size());
    std::unordered_map<::eyelib::world::EntityId, ::eyelib::world::components::Script,
                       ::eyelib::world::EntityIdHash> scriptData;
    scriptData.reserve(scripts.size());

    for (const auto& entry : scripts) {
        active[entry.entity] = entry.component.isEnabled;
        scriptData.emplace(entry.entity, entry.component);
        if (entry.component.isEnabled && entry.component.needsReload) {
            LoadOrReload(world, entry, assetRoots, true);
        }
    }

    for (auto it = m_bindings.begin(); it != m_bindings.end(); ) {
        if (active.find(it->first) == active.end() || !active[it->first]) {
            eyelib::api::Script::CallStop(it->second.handle);
            if (it->second.handle != eyelib::scripting::InvalidScriptHandle) {
                eyelib::api::Script::Unload(it->second.handle);
            }
            it = m_bindings.erase(it);
            continue;
        }
        ++it;
    }

    for (const auto& [entity, binding] : m_bindings) {
        (void)entity;
        if (binding.handle != eyelib::scripting::InvalidScriptHandle) {
            eyelib::api::Script::CallUpdate(binding.handle, deltaTime);
            auto scriptIt = scriptData.find(entity);
            if (scriptIt != scriptData.end()) {
                if (auto* state = eyelib::api::Script::GetState(binding.handle)) {
                    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue> props;
                    if (scripting::ReadProperties(state, eyelib::api::Script::GetName(binding.handle), props)) {
                        if (!scripting::PropertiesEqual(scriptIt->second.properties, props)) {
                            auto updated = scriptIt->second;
                            updated.properties = std::move(props);
                            world.SetScript(entity, updated);
                        }
                    }
                }
            }
        }
    }
}

void WorldScriptingBridge::SetPhysicsBridge(WorldPhysicsBridge* physics) {
    m_physics = physics;
}

void WorldScriptingBridge::SetAnimationBridge(WorldAnimationBridge* animation) {
    m_animation = animation;
}

} // namespace openeye::player
