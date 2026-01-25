#include "api/render/Shader.h"
#include "eyelib/render/resources/Shader.h"

namespace eyelib {
namespace api {

class Shader::Impl {
public:
    std::shared_ptr<::eyelib::render::Shader> shader = std::make_shared<::eyelib::render::Shader>();
};

Shader::Shader() : m_impl(std::make_unique<Impl>()) {}

Shader::~Shader() = default;

Shader::Shader(Shader&& other) noexcept = default;

Shader& Shader::operator=(Shader&& other) noexcept = default;

bool Shader::BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    return m_impl->shader->BuildFromSource(vertexSrc, fragmentSrc);
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    return m_impl->shader->LoadFromFile(vertexPath, fragmentPath);
}

void Shader::Destroy() {
    m_impl->shader->Destroy();
}

void Shader::Bind() const {
    m_impl->shader->Bind();
}

void Shader::Unbind() const {
    m_impl->shader->Unbind();
}

void Shader::SetBool(const std::string& name, bool value) const {
    m_impl->shader->SetBool(name, value);
}

void Shader::SetInt(const std::string& name, int value) const {
    m_impl->shader->SetInt(name, value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    m_impl->shader->SetFloat(name, value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    m_impl->shader->SetVec2(name, value);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    m_impl->shader->SetVec3(name, value);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    m_impl->shader->SetVec4(name, value);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    m_impl->shader->SetMat4(name, value);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    m_impl->shader->SetMat4Array(name, values, count);
}

bool Shader::IsValid() const {
    return m_impl->shader->IsValid();
}

std::shared_ptr<::eyelib::render::Shader> Shader::GetInternal() const {
    return m_impl->shader;
}

std::shared_ptr<Shader> Shader::WrapInternal(const std::shared_ptr<::eyelib::render::Shader>& shader) {
    auto apiShader = std::make_shared<Shader>();
    if (shader) apiShader->m_impl->shader = shader;
    return apiShader;
}

} // namespace api
} // namespace eyelib
