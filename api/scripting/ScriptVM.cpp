#include "api/scripting/ScriptVM.h"
#include "eyelib/scripting/LuaVM.h"

namespace eyelib {
namespace api {

class ScriptVM::Impl {
public:
    std::shared_ptr<::eyelib::scripting::LuaVM> vm;
    
    static SandboxMode ConvertSandboxMode(::eyelib::scripting::SandboxMode mode) {
        switch (mode) {
            case ::eyelib::scripting::SandboxMode::Restricted: return SandboxMode::Restricted;
            case ::eyelib::scripting::SandboxMode::Standard: return SandboxMode::Standard;
            case ::eyelib::scripting::SandboxMode::Permissive: return SandboxMode::Permissive;
            default: return SandboxMode::Restricted;
        }
    }
    
    static ::eyelib::scripting::SandboxMode ConvertSandboxMode(SandboxMode mode) {
        switch (mode) {
            case SandboxMode::Restricted: return ::eyelib::scripting::SandboxMode::Restricted;
            case SandboxMode::Standard: return ::eyelib::scripting::SandboxMode::Standard;
            case SandboxMode::Permissive: return ::eyelib::scripting::SandboxMode::Permissive;
            default: return ::eyelib::scripting::SandboxMode::Restricted;
        }
    }
};

ScriptVM::ScriptVM() : m_impl(std::make_unique<Impl>()) {
    m_impl->vm = ::eyelib::scripting::LuaVM::Create();
}

ScriptVM::~ScriptVM() = default;

ScriptVM::ScriptVM(ScriptVM&& other) noexcept = default;

ScriptVM& ScriptVM::operator=(ScriptVM&& other) noexcept = default;

std::shared_ptr<ScriptVM> ScriptVM::Create() {
    auto vm = std::make_shared<ScriptVM>();
    return vm;
}

bool ScriptVM::IsValid() const {
    return m_impl->vm && m_impl->vm->IsValid();
}

void ScriptVM::SetSandboxMode(SandboxMode mode) {
    if (m_impl->vm) {
        m_impl->vm->SetSandboxMode(Impl::ConvertSandboxMode(mode));
    }
}

bool ScriptVM::ExecuteString(const std::string& code, std::string& error) {
    if (!m_impl->vm) {
        error = "VM is not valid";
        return false;
    }
    return m_impl->vm->ExecuteString(code, error);
}

bool ScriptVM::ExecuteFile(const std::string& path, std::string& error) {
    if (!m_impl->vm) {
        error = "VM is not valid";
        return false;
    }
    return m_impl->vm->ExecuteFile(path, error);
}

void ScriptVM::SetMemoryLimit(size_t bytes) {
    if (m_impl->vm) {
        m_impl->vm->SetMemoryLimit(bytes);
    }
}

size_t ScriptVM::GetMemoryUsage() const {
    if (!m_impl->vm) {
        return 0;
    }
    return m_impl->vm->GetMemoryUsage();
}

void ScriptVM::SetErrorCallback(std::function<void(const std::string&)> callback) {
    if (m_impl->vm) {
        m_impl->vm->SetErrorCallback(callback);
    }
}

} // namespace api
} // namespace eyelib
