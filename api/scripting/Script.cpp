#include "api/scripting/Script.h"
#include "eyelib/scripting/ScriptManager.h"

namespace eyelib {
namespace api {

class Script::Impl {
public:
    // PIMPL implementation - currently just wraps ScriptManager
    // Future: Can add caching, validation, etc. without breaking ABI
};

Script::Script() : m_impl(std::make_unique<Impl>()) {}

Script::~Script() = default;

Script::Script(Script&& other) noexcept = default;

Script& Script::operator=(Script&& other) noexcept = default;

scripting::ScriptHandle Script::Load(const std::string& path) {
    return scripting::ScriptManager::Get().LoadScript(path);
}

void Script::Unload(scripting::ScriptHandle handle) {
    scripting::ScriptManager::Get().UnloadScript(handle);
}

bool Script::Reload(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().ReloadScript(handle);
}

std::string Script::GetPath(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().GetScriptPath(handle);
}

std::string Script::GetName(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().GetScriptName(handle);
}

bool Script::IsValid(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().IsScriptValid(handle);
}

std::string Script::GetError(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().GetScriptError(handle);
}

} // namespace api
} // namespace eyelib
