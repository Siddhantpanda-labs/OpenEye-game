#include "Plugin.h"
#include "eyelib/core/plugin/PluginManager.h"

namespace api {
namespace plugin {

struct PluginManager::Impl {
    eyelib::core::plugin::PluginManager mgr;
};

PluginManager::PluginManager() : m_impl(new Impl) {}

PluginManager::~PluginManager() {
    delete m_impl;
    m_impl = nullptr;
}

PluginManager::PluginManager(PluginManager&& other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

PluginManager& PluginManager::operator=(PluginManager&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

bool PluginManager::LoadPlugin(const std::string& path) {
    return m_impl->mgr.LoadPlugin(path);
}

void PluginManager::UnloadPlugin(const std::string& name) {
    m_impl->mgr.UnloadPlugin(name);
}

IPlugin* PluginManager::GetPlugin(const std::string& name) const {
    return m_impl->mgr.GetPlugin(name);
}

void PluginManager::RegisterInterface(const std::string& name, void* interfacePtr) {
    m_impl->mgr.RegisterInterface(name, interfacePtr);
}

void* PluginManager::GetInterface(const std::string& name) const {
    return m_impl->mgr.GetInterface(name);
}

} // namespace plugin
} // namespace api
