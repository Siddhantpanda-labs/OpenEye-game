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

bool Script::CallStart(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().CallStart(handle);
}

bool Script::CallUpdate(scripting::ScriptHandle handle, float deltaTime) {
    return scripting::ScriptManager::Get().CallUpdate(handle, deltaTime);
}

bool Script::CallStop(scripting::ScriptHandle handle) {
    return scripting::ScriptManager::Get().CallStop(handle);
}

lua_State* Script::GetState(scripting::ScriptHandle handle) {
    auto vm = scripting::ScriptManager::Get().GetScriptVM(handle);
    return vm ? vm->GetState() : nullptr;
}

} // namespace api
} // namespace eyelib
