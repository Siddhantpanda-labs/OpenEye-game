#include "api/rendering/Shader.h"

#include "api/rendering/internal/ResourceAccess.h"
#include "eyelib/rendering/resources/Shader.h"

#include <utility>

namespace eyelib {
namespace api {
namespace rendering {

class Shader::Impl {
public:
    std::shared_ptr<eyelib::rendering::Shader> shader;
    // Cached friendly name for `GetName()`. Set at Wrap time; the internal Shader has no
    // GetName() of its own (asset path is recoverable via GetDescriptor() but expensive
    // to copy each call). M3 factory will pass an asset name through `Wrap`.
    std::string name;
};

Shader::Shader() noexcept : m_impl(std::make_unique<Impl>()) {}
Shader::~Shader() = default;

Shader::Shader(const Shader& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
Shader& Shader::operator=(const Shader& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
Shader::Shader(Shader&& other) noexcept = default;
Shader& Shader::operator=(Shader&& other) noexcept = default;

bool Shader::IsValid() const noexcept {
    return m_impl && m_impl->shader != nullptr;
}

std::string Shader::GetName() const {
    return (m_impl ? m_impl->name : std::string{});
}

Shader ShaderAccess::Wrap(std::shared_ptr<eyelib::rendering::Shader> internalShader) {
    Shader handle{};
    handle.m_impl->shader = std::move(internalShader);
    // M3 factory will provide a name via a future `Wrap` overload; v1 leaves it empty.
    return handle;
}

} // namespace rendering
} // namespace api
} // namespace eyelib
