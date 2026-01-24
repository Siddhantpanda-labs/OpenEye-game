#include "core/plugin/PluginManager.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace eyelib {
namespace core {
namespace plugin {

namespace {

void* LoadLib(const char* path) {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryA(path));
#else
    return dlopen(path, RTLD_LAZY);
#endif
}

void FreeLib(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* GetSymbol(void* handle, const char* name) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

} // namespace

PluginManager::~PluginManager() {
    std::vector<std::string> names;
    names.reserve(m_plugins.size());
    for (const auto& [n, p] : m_plugins)
        names.push_back(n);
    for (const auto& n : names)
        UnloadPlugin(n);
}

bool PluginManager::LoadPlugin(const std::string& path) {
    void* handle = LoadLib(path.c_str());
    if (!handle) return false;

    using CreatePluginFn = IPlugin* (*)();
    auto* create = reinterpret_cast<CreatePluginFn>(GetSymbol(handle, "CreatePlugin"));
    if (!create) {
        FreeLib(handle);
        return false;
    }

    IPlugin* raw = create();
    if (!raw) {
        FreeLib(handle);
        return false;
    }

    std::unique_ptr<IPlugin> plugin(raw);
    if (!plugin->Initialize()) {
        plugin->Shutdown();
        FreeLib(handle);
        return false;
    }

    std::string name = plugin->GetName();
    LoadedPlugin lp;
    lp.handle = handle;
    lp.plugin = std::move(plugin);
    m_plugins[std::move(name)] = std::move(lp);
    return true;
}

void PluginManager::UnloadPlugin(const std::string& name) {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) return;
    it->second.plugin->Shutdown();
    FreeLib(it->second.handle);
    m_plugins.erase(it);
}

IPlugin* PluginManager::GetPlugin(const std::string& name) const {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) return nullptr;
    return it->second.plugin.get();
}

void PluginManager::RegisterInterface(const std::string& name, void* interfacePtr) {
    m_interfaces[name] = interfacePtr;
}

void* PluginManager::GetInterface(const std::string& name) const {
    auto it = m_interfaces.find(name);
    if (it == m_interfaces.end()) return nullptr;
    return it->second;
}

} // namespace plugin
} // namespace core
} // namespace eyelib
